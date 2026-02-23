#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../ui/ruby_utils.h"
#include <map>
#include <string>

TEST_CASE("RemoveRubyAnnotations") {
    SUBCASE("Basic annotation removal") {
        const char* input = "[Main:Ruby]";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "Main");
        CHECK(result.second.size() == 1);
        CHECK(result.second["Main"] == "Ruby");
    }

    SUBCASE("Text with surrounding context") {
        const char* input = "Prefix [Main:Ruby] Suffix";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "Prefix Main Suffix");
        CHECK(result.second.size() == 1);
        CHECK(result.second["Main"] == "Ruby");
    }

    SUBCASE("Multiple annotations") {
        const char* input = "[First:1st] and [Second:2nd]";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "First and Second");
        CHECK(result.second.size() == 2);
        CHECK(result.second["First"] == "1st");
        CHECK(result.second["Second"] == "2nd");
    }

    SUBCASE("No annotations") {
        const char* input = "Just some text";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "Just some text");
        CHECK(result.second.empty());
    }

    SUBCASE("Empty string") {
        const char* input = "";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "");
        CHECK(result.second.empty());
    }

    SUBCASE("Malformed: missing closing bracket") {
        // Based on implementation analysis:
        // [Main:Ruby -> Main, map{Main:Ruby}
        const char* input = "[Main:Ruby";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "Main");
        CHECK(result.second.size() == 1);
        CHECK(result.second["Main"] == "Ruby");
    }

    SUBCASE("Malformed: missing colon") {
        // [Main -> Main, map empty
        const char* input = "[Main";
        auto result = RemoveRubyAnnotations(input);
        CHECK(result.first == "Main");
        CHECK(result.second.empty());
    }
}

TEST_CASE("ReAddRubyAnnotations") {
    SUBCASE("Basic re-addition") {
        std::map<std::string, std::string> rubyMap;
        rubyMap["Main"] = "Ruby";
        std::string input = "Main";
        auto result = ReAddRubyAnnotations(input, rubyMap);
        CHECK(result == "[Main:Ruby]");
    }

    SUBCASE("Context preservation") {
        std::map<std::string, std::string> rubyMap;
        rubyMap["Main"] = "Ruby";
        std::string input = "Prefix Main Suffix";
        auto result = ReAddRubyAnnotations(input, rubyMap);
        CHECK(result == "Prefix [Main:Ruby] Suffix");
    }

    SUBCASE("Multiple re-additions") {
        std::map<std::string, std::string> rubyMap;
        rubyMap["First"] = "1st";
        rubyMap["Second"] = "2nd";
        std::string input = "First and Second";
        auto result = ReAddRubyAnnotations(input, rubyMap);
        CHECK(result == "[First:1st] and [Second:2nd]");
    }

    SUBCASE("No matches") {
        std::map<std::string, std::string> rubyMap;
        rubyMap["Other"] = "Ruby";
        std::string input = "Main";
        auto result = ReAddRubyAnnotations(input, rubyMap);
        CHECK(result == "Main");
    }
}
