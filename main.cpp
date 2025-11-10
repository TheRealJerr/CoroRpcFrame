#include <print>
#include <json/json.h>

auto main() -> int {
    Json::Value root;
    root["Hello"] = "World";
    std::stringstream ssm;
    Json::StreamWriterBuilder().newStreamWriter()->write(root, &ssm);
    std::print("{}\n", &ssm);
}
