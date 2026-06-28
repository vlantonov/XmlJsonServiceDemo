#include <future>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "xmljson/config.hpp"
#include "xmljson/converter.hpp"
#include "xmljson/http_server.hpp"

namespace {

class Integration : public ::testing::Test {
 protected:
	void SetUp() override {
		cfg_.host = "127.0.0.1";
		cfg_.port = 0;
		cfg_.base_threads = 4;
		cfg_.max_body_bytes = 1024 * 1024;

		converter_.reset(new xmljson::Converter());
		server_.reset(new xmljson::HttpServer(cfg_, *converter_));
		ASSERT_TRUE(server_->start_background());
		ASSERT_GT(server_->bound_port(), 0);
		port_ = server_->bound_port();
	}

	void TearDown() override {
		if (server_) {
			server_->stop();
		}
		server_.reset();
		converter_.reset();
	}

	std::unique_ptr<httplib::Client> make_client() {
		auto c = std::make_unique<httplib::Client>("127.0.0.1", port_);
		c->set_read_timeout(10, 0);
		return c;
	}

	static bool has_prefix(const std::string& value, const std::string& prefix) {
		return value.rfind(prefix, 0) == 0;
	}

	xmljson::ServerConfig cfg_;
	std::unique_ptr<xmljson::Converter> converter_;
	std::unique_ptr<xmljson::HttpServer> server_;
	int port_ = 0;
};

// cppcheck-suppress syntaxError
TEST_F(Integration, RealisticDocument_RoundTrips_Via_Http) {
	const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<invoice id="INV-100" currency="EUR">
  <customer><name>Ada</name><email>ada@example.org</email></customer>
  <items>
	<item sku="A1"><description>Widget</description><quantity>2</quantity><unit_price>9.99</unit_price></item>
	<item sku="A2"><description>Gadget</description><quantity>1</quantity><unit_price>19.50</unit_price></item>
  </items>
  <total>39.48</total>
</invoice>)";

	auto client = make_client();
	const auto xml_to_json = client->Post("/convert", xml, "application/xml");
	ASSERT_TRUE(xml_to_json);
	ASSERT_EQ(xml_to_json->status, 200);
	EXPECT_TRUE(has_prefix(xml_to_json->get_header_value("Content-Type"), "application/json"));

	const nlohmann::json json_body = nlohmann::json::parse(xml_to_json->body);
	EXPECT_EQ(json_body.at("invoice").at("@id"), "INV-100");
	EXPECT_EQ(json_body.at("invoice").at("@currency"), "EUR");
	EXPECT_EQ(json_body.at("invoice").at("customer").at("name"), "Ada");
	ASSERT_TRUE(json_body.at("invoice").at("items").at("item").is_array());
	ASSERT_EQ(json_body.at("invoice").at("items").at("item").size(), 2);
	EXPECT_EQ(json_body.at("invoice").at("items").at("item").at(0).at("@sku"), "A1");
	EXPECT_EQ(json_body.at("invoice").at("total"), "39.48");

	const auto json_to_xml = client->Post("/convert", json_body.dump(), "application/json");
	ASSERT_TRUE(json_to_xml);
	ASSERT_EQ(json_to_xml->status, 200);
	EXPECT_NE(json_to_xml->get_header_value("Content-Type").find("xml"), std::string::npos);

	pugi::xml_document doc;
	const pugi::xml_parse_result parsed = doc.load_string(json_to_xml->body.c_str());
	ASSERT_TRUE(parsed) << parsed.description() << " at offset " << parsed.offset;

	const pugi::xml_node invoice = doc.document_element();
	ASSERT_TRUE(invoice);
	EXPECT_STREQ(invoice.name(), "invoice");
	EXPECT_STREQ(invoice.attribute("id").value(), "INV-100");
	EXPECT_STREQ(invoice.child("customer").child("name").child_value(), "Ada");

	const pugi::xml_node items = invoice.child("items");
	ASSERT_TRUE(items);
	int item_count = 0;
	for (pugi::xml_node item = items.child("item"); item; item = item.next_sibling("item")) {
		++item_count;
		if (item_count == 1) {
			EXPECT_STREQ(item.attribute("sku").value(), "A1");
		} else if (item_count == 2) {
			EXPECT_STREQ(item.attribute("sku").value(), "A2");
		}
	}
	EXPECT_EQ(item_count, 2);
}

TEST_F(Integration, Concurrent_Mixed_Conversions_All_Succeed) {
	const std::string xml_payload = "<order><id>42</id></order>";
	const std::string json_payload = R"({"order":{"id":"42"}})";

	std::vector<bool> xml_to_json_direction(32, false);
	for (int i = 0; i < 16; ++i) {
		xml_to_json_direction[static_cast<std::size_t>(i)] = true;
	}
	std::mt19937 rng(123456u);
	std::shuffle(xml_to_json_direction.begin(), xml_to_json_direction.end(), rng);

	std::vector<std::future<bool>> tasks;
	tasks.reserve(32);
	const int current_port = port_;

	for (int i = 0; i < 32; ++i) {
		const bool run_xml_to_json = xml_to_json_direction[static_cast<std::size_t>(i)];
		tasks.push_back(std::async(std::launch::async, [current_port, run_xml_to_json, xml_payload, json_payload]() {
			httplib::Client client("127.0.0.1", current_port);
			client.set_read_timeout(10, 0);

			if (run_xml_to_json) {
				const auto res = client.Post("/convert", xml_payload, "application/xml");
				if (!res || res->status != 200) {
					return false;
				}
				const nlohmann::json body = nlohmann::json::parse(res->body);
				return body.contains("order") && body.at("order").at("id") == "42";
			}

			const auto res = client.Post("/convert", json_payload, "application/json");
			if (!res || res->status != 200) {
				return false;
			}
			pugi::xml_document doc;
			const pugi::xml_parse_result parsed = doc.load_string(res->body.c_str());
			if (!parsed) {
				return false;
			}
			return std::string(doc.document_element().name()) == "order";
		}));
	}

	for (auto& task : tasks) {
		EXPECT_TRUE(task.get());
	}
}

TEST_F(Integration, Health_And_Version_Endpoints_Reachable) {
	auto client = make_client();

	const auto health = client->Get("/healthz");
	ASSERT_TRUE(health);
	EXPECT_EQ(health->status, 200);
	const nlohmann::json health_body = nlohmann::json::parse(health->body);
	EXPECT_EQ(health_body.at("status"), "ok");

	const auto version = client->Get("/version");
	ASSERT_TRUE(version);
	EXPECT_EQ(version->status, 200);
	const nlohmann::json version_body = nlohmann::json::parse(version->body);
	EXPECT_EQ(version_body.at("name"), "xmljson");
	EXPECT_EQ(version_body.at("version"), "0.1.0");
}

TEST_F(Integration, Malformed_Input_Maps_To_400_With_Error_Envelope) {
	auto client = make_client();
	const auto res = client->Post("/convert", "<a><b></a>", "application/xml");

	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 400);

	const nlohmann::json body = nlohmann::json::parse(res->body);
	EXPECT_EQ(body.at("error"), "MalformedInput");
}

TEST_F(Integration, Server_Restartable) {
	ASSERT_TRUE(server_);
	server_->stop();
	server_.reset();
	port_ = 0;

	server_ = std::make_unique<xmljson::HttpServer>(cfg_, *converter_);
	ASSERT_TRUE(server_->start_background());
	ASSERT_GT(server_->bound_port(), 0);
	port_ = server_->bound_port();

	auto client = make_client();
	const auto health = client->Get("/healthz");
	ASSERT_TRUE(health);
	EXPECT_EQ(health->status, 200);

	const nlohmann::json body = nlohmann::json::parse(health->body);
	EXPECT_EQ(body.at("status"), "ok");
}

}  // namespace