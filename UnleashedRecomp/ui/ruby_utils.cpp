#include "ruby_utils.h"

std::pair<std::string, std::map<std::string, std::string>> RemoveRubyAnnotations(const char* input)
{
    std::string output;
    std::map<std::string, std::string> rubyMap;
    std::string currentMain, currentRuby;
    size_t idx = 0;

    while (input[idx] != '\0')
    {
        if (input[idx] == '[')
        {
            idx++;
            currentMain.clear();
            currentRuby.clear();

            while (input[idx] != ':' && input[idx] != ']' && input[idx] != '\0')
            {
                currentMain += input[idx++];
            }
            if (input[idx] == ':')
            {
                idx++;
                while (input[idx] != ']' && input[idx] != '\0')
                {
                    currentRuby += input[idx++];
                }
            }
            if (input[idx] == ']')
            {
                idx++;
            }

            if (!currentMain.empty() && !currentRuby.empty())
            {
                rubyMap[currentMain] = currentRuby;
            }

            output += currentMain;
        }
        else
        {
            output += input[idx++];
        }
    }

    return { output, rubyMap };
}

std::string ReAddRubyAnnotations(const std::string_view& wrappedText, const std::map<std::string, std::string>& rubyMap)
{
    std::string annotatedText;
    size_t idx = 0;
    size_t length = wrappedText.length();

    while (idx < length) {
        bool matched = false;
        for (const auto& [mainText, rubyText] : rubyMap)
        {
            if (wrappedText.substr(idx, mainText.length()) == mainText)
            {
                annotatedText += "[";
                annotatedText += mainText;
                annotatedText += ":";
                annotatedText += rubyText;
                annotatedText += "]";

                idx += mainText.length();
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            annotatedText += wrappedText[idx++];
        }
    }

    return annotatedText;
}
