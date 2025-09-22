#include <catch2/catch_all.hpp>
#include <confy/Config.hpp>
#include <confy/Util.hpp>
#include <fstream>

using namespace confy;
using nlohmann::json;

TEST_CASE("load JSON") {
    std::string path = "tmp_cfg.json";
    json j = {{"auth", {{"local", {{"enabled", false}}}}}};
    std::ofstream(path) << j.dump(2);
    LoadOptions opts;
    opts.file_path = path;
    Config cfg = Config::load(opts);
    REQUIRE(cfg.at("auth.local.enabled").get<bool>() == false);
    std::remove(path.c_str());
}

TEST_CASE("load TOML") {
    std::string path = "tmp_cfg.toml";
    std::ofstream(path) << "[auth.local]\nenabled = false\n";
    LoadOptions opts;
    opts.file_path = path;
    Config cfg = Config::load(opts);
    REQUIRE(cfg.at("auth.local.enabled").get<bool>() == false);
    std::remove(path.c_str());
}

TEST_CASE("env override") {
#ifdef _WIN32
    _putenv("APP_CONF_AUTH_LOCAL_ENABLED=true");
#else
    setenv("APP_CONF_AUTH_LOCAL_ENABLED", "true", 1);
#endif
    json j = {{"auth", {{"local", {{"enabled", false}}}}}};
    LoadOptions opts;
    opts.defaults = j;
    opts.prefix = "APP_CONF";
    Config cfg = Config::load(opts);
    REQUIRE(cfg.at("auth.local.enabled").get<bool>() == true);
}

TEST_CASE("dict override") {
    json j = {{"auth", {{"local", {{"enabled", false}}}}}};
    LoadOptions opts;
    opts.defaults = j;
    opts.overrides = {{"auth.local.enabled", true}};
    Config cfg = Config::load(opts);
    REQUIRE(cfg.at("auth.local.enabled").get<bool>() == true);
}

TEST_CASE("defaults and mandatory") {
    LoadOptions opts;
    opts.defaults = json{{"db", json::object()}};
    opts.mandatory = {"db.host"};
    REQUIRE_THROWS_AS(Config::load(opts), MissingMandatoryConfig);

    opts.defaults = json{{"db", {{"host","127.0.0.1"}}}};
    Config cfg = Config::load(opts);
    REQUIRE(cfg.at("db.host").get<std::string>() == "127.0.0.1");
}
