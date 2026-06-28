#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "xmljson/converter.hpp"
#include "xmljson/http_server.hpp"

namespace {

class HttpHandlersTest : public ::testing::Test {
 protected:
	void SetUp() override {
		cfg_.host = "127.0.0.1";
		cfg_.port = 0;
		cfg_.base_threads = 2;
		cfg_.max_body_bytes = 8 * 1024;

		converter_ = std::make_unique<xmljson::Converter>();
		server_ = std::make_unique<xmljson::HttpServer>(cfg_, *converter_);
		ASSERT_TRUE(server_->start_background());
		ASSERT_GT(server_->bound_port(), 0);

		client_ = std::make_unique<httplib::Client>("127.0.0.1", server_->bound_port());
		client_->set_read_timeout(5, 0);
	}

	void TearDown() override {
		if (server_) {
			server_->stop();
		}
		client_.reset();
		server_.reset();
		converter_.reset();
	}

	static bool has_prefix(const std::string& value, const std::string& prefix) {
		return value.rfind(prefix, 0) == 0;
	}

	static nlohmann::json parse_json_body(const httplib::Result& res) {
		return nlohmann::json::parse(res->body);
	}

	static pugi::xml_document parse_xml_body(const httplib::Result& res) {
		pugi::xml_document doc;
		const pugi::xml_parse_result parsed = doc.load_string(res->body.c_str());
		EXPECT_TRUE(parsed) << parsed.description() << " at offset " << parsed.offset;
		return doc;
	}

	xmljson::ServerConfig cfg_;
	std::unique_ptr<xmljson::Converter> converter_;
	std::unique_ptr<xmljson::HttpServer> server_;
	std::unique_ptr<httplib::Client> client_;
};

TEST_F(HttpHandlersTest, Healthz_Returns_Ok) {
	const auto res = client_->Get("/healthz");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("status"), "ok");
}

TEST_F(HttpHandlersTest, Version_Returns_Name_And_Version) {
	const auto res = client_->Get("/version");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("name"), "xmljson");
	EXPECT_EQ(body.at("version"), "0.1.0");
}

TEST_F(HttpHandlersTest, Convert_Xml_To_Json_With_XmlContentType) {
	const auto res = client_->Post("/convert", "<a>hi</a>", "application/xml");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);
	EXPECT_TRUE(has_prefix(res->get_header_value("Content-Type"), "application/json"));

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("a"), "hi");
}

TEST_F(HttpHandlersTest, Convert_TextXml_ContentType_Also_Works) {
	const auto res = client_->Post("/convert", "<a>hi</a>", "text/xml");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("a"), "hi");
}

TEST_F(HttpHandlersTest, Convert_PlusXml_Suffix_Also_Works) {
	const auto res = client_->Post("/convert", "<a>hi</a>", "application/atom+xml");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("a"), "hi");
}

TEST_F(HttpHandlersTest, Convert_Json_To_Xml_With_JsonContentType) {
	const auto res = client_->Post("/convert", R"({"a":"hi"})", "application/json");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);
	EXPECT_NE(res->get_header_value("Content-Type").find("xml"), std::string::npos);

	const pugi::xml_document doc = parse_xml_body(res);
	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "a");
	EXPECT_STREQ(root.child_value(), "hi");
}

TEST_F(HttpHandlersTest, Convert_PlusJson_Suffix_Also_Works) {
	const auto res = client_->Post("/convert", R"({"a":"hi"})", "application/vnd.foo+json");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const pugi::xml_document doc = parse_xml_body(res);
	EXPECT_STREQ(doc.document_element().name(), "a");
}

TEST_F(HttpHandlersTest, Convert_Unsupported_ContentType_Returns_415) {
	const auto res = client_->Post("/convert", "<a>hi</a>", "text/plain");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 415);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("error"), "UnsupportedMediaType");
	EXPECT_EQ(body.at("path"), "/convert");
}

TEST_F(HttpHandlersTest, Convert_Malformed_Xml_Returns_400) {
	const auto res = client_->Post("/convert", "<a><b></a>", "application/xml");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 400);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("error"), "MalformedInput");
	EXPECT_EQ(body.at("path"), "/convert");
}

TEST_F(HttpHandlersTest, Convert_Malformed_Json_Returns_400) {
	const auto res = client_->Post("/convert", "{", "application/json");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 400);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("error"), "MalformedInput");
	EXPECT_EQ(body.at("path"), "/convert");
}

TEST_F(HttpHandlersTest, Convert_MultiKey_Json_Returns_400_UnsupportedShape) {
	const auto res = client_->Post("/convert", R"({"a":1,"b":2})", "application/json");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 400);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("error"), "UnsupportedShape");
	EXPECT_EQ(body.at("path"), "/convert");
}

TEST_F(HttpHandlersTest, XmlToJson_Endpoint_Ignores_ContentType) {
	const auto res = client_->Post("/xml-to-json", "<a/>", "text/plain");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body, nlohmann::json::parse(R"({"a":null})"));
}

TEST_F(HttpHandlersTest, JsonToXml_Endpoint_Ignores_ContentType) {
	const auto res = client_->Post("/json-to-xml", R"({"a":"x"})", "text/plain");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 200);

	const pugi::xml_document doc = parse_xml_body(res);
	EXPECT_STREQ(doc.document_element().name(), "a");
	EXPECT_STREQ(doc.document_element().child_value(), "x");
}

TEST_F(HttpHandlersTest, Body_Over_Limit_Returns_413) {
	std::string body;
	body.reserve(16 * 1024 + 64);
	body += "<a>";
	body.append(16 * 1024, 'x');
	body += "</a>";

	const auto res = client_->Post("/xml-to-json", body, "application/xml");

	if (res) {
		EXPECT_EQ(res->status, 413);
		const nlohmann::json parsed = parse_json_body(res);
		EXPECT_EQ(parsed.at("error"), "PayloadTooLarge");
		EXPECT_EQ(parsed.at("path"), "/xml-to-json");
	} else {
		SUCCEED() << "cpp-httplib closed the connection for oversized payload";
	}
}

TEST_F(HttpHandlersTest, Unknown_Path_Returns_404) {
	const auto res = client_->Get("/nope");
	ASSERT_TRUE(res);
	EXPECT_EQ(res->status, 404);

	const nlohmann::json body = parse_json_body(res);
	EXPECT_EQ(body.at("error"), "NotFound");
	EXPECT_EQ(body.at("path"), "/nope");
}

TEST_F(HttpHandlersTest, Concurrent_Requests_All_Succeed) {
	std::vector<std::future<bool>> tasks;
	tasks.reserve(16);

	for (int i = 0; i < 16; ++i) {
		const int port = server_->bound_port();
		tasks.push_back(std::async(std::launch::async, [port]() {
			httplib::Client client("127.0.0.1", port);
			client.set_read_timeout(5, 0);

			const auto res = client.Post("/convert", "<a>hi</a>", "application/xml");
			if (!res || res->status != 200) {
				return false;
			}

			const nlohmann::json body = nlohmann::json::parse(res->body);
			return body.contains("a") && body.at("a") == "hi";
		}));
	}

	for (auto& task : tasks) {
		EXPECT_TRUE(task.get());
	}
}

}  // namespace