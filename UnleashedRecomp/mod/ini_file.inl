inline size_t IniFile::hashStr(const std::string_view& str)
{
    return XXH3_64bits(str.data(), str.size());
}

inline bool IniFile::isWhitespace(char value)
{
    return value == ' ' || value == '\t';
}

inline bool IniFile::isNewLine(char value)
{
    return value == '\n' || value == '\r';
}

inline bool IniFile::read(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.good())
        return false;

    file.seekg(0, std::ios::end);

    const size_t dataSize = static_cast<size_t>(file.tellg());
    const auto data = std::make_unique<char[]>(dataSize + 1);
    data[dataSize] = '\0';

    file.seekg(0, std::ios::beg);
    file.read(data.get(), dataSize);

    file.close();

    const char* p = data.get();
    const char* end = p + dataSize;
    Section* section = nullptr;

    while (p < end)
    {
        // Skip leading whitespace
        while (p < end && isWhitespace(*p))
            ++p;

        if (p >= end)
            break;

        const char c = *p;

        if (isNewLine(c))
        {
            ++p;
            continue;
        }

        if (c == ';')
        {
            // Comment: skip until newline
            ++p;
            while (p < end && !isNewLine(*p))
                ++p;
        }
        else if (c == '[')
        {
            // Section
            const char* start = ++p;
            while (p < end && !isNewLine(*p) && *p != ']')
                ++p;

            if (p < end && *p == ']')
            {
                std::string_view sectionNameView(start, p - start);
                size_t hash = hashStr(sectionNameView);

                // Use default construction if not found, then fill name
                section = &m_sections[hash];
                if (section->name.empty())
                    section->name = sectionNameView;

                ++p; // Skip ']'
            }
            else
            {
                // Malformed section header (missing ']' or newline before ']')
                return false;
            }
        }
        else
        {
            // Property
            if (section == nullptr)
                return false;

            const char* keyStart;
            const char* keyEnd;

            if (c == '"')
            {
                keyStart = ++p;
                while (p < end && !isNewLine(*p) && *p != '"')
                    ++p;

                if (p >= end || *p != '"')
                    return false;

                keyEnd = p++; // Store end and skip quote
            }
            else
            {
                keyStart = p;
                while (p < end && !isNewLine(*p) && !isWhitespace(*p) && *p != '=')
                    ++p;
                keyEnd = p;
            }

            std::string_view keyView(keyStart, keyEnd - keyStart);

            // Skip until '=' or newline to handle spaces/garbage between key and '='
            while (p < end && !isNewLine(*p) && *p != '=')
                ++p;

            // Always add property, even if '=' is missing (empty value)
            // This matches the behavior of the original implementation which creates an entry
            // immediately after parsing the name.
            size_t keyHash = hashStr(keyView);
            auto& property = section->properties[keyHash];

            if (property.name.empty())
                property.name = keyView;

            if (p < end && *p == '=')
            {
                ++p; // Skip '='

                // Skip whitespace after '='
                while (p < end && isWhitespace(*p))
                    ++p;

                const char* valStart;
                const char* valEnd;

                if (p < end && *p == '"')
                {
                    valStart = ++p;
                    while (p < end && !isNewLine(*p) && *p != '"')
                        ++p;

                    if (p >= end || *p != '"')
                        return false;

                    valEnd = p++; // Store end and skip quote
                }
                else
                {
                    valStart = p;
                    // Current behavior: stop at whitespace
                    while (p < end && !isNewLine(*p) && !isWhitespace(*p))
                        ++p;
                    valEnd = p;
                }

                std::string_view valView(valStart, valEnd - valStart);
                property.value = valView;
            }
        }
    }

    return true;
}

inline std::string IniFile::getString(const std::string_view& sectionName, const std::string_view& propertyName, std::string defaultValue) const
{
    const auto sectionPair = m_sections.find(hashStr(sectionName));
    if (sectionPair != m_sections.end())
    {
        const auto propertyPair = sectionPair->second.properties.find(hashStr(propertyName));
        if (propertyPair != sectionPair->second.properties.end())
            return propertyPair->second.value;
    }
    return defaultValue;
}

inline bool IniFile::getBool(const std::string_view& sectionName, const std::string_view& propertyName, bool defaultValue) const
{
    const auto sectionPair = m_sections.find(hashStr(sectionName));
    if (sectionPair != m_sections.end())
    {
        const auto propertyPair = sectionPair->second.properties.find(hashStr(propertyName));
        if (propertyPair != sectionPair->second.properties.end() && !propertyPair->second.value.empty())
        {
            const char firstChar = propertyPair->second.value[0];
            return firstChar == 't' || firstChar == 'T' || firstChar == 'y' || firstChar == 'Y' || firstChar == '1';
        }
    }
    return defaultValue;
}

inline bool IniFile::contains(const std::string_view& sectionName) const
{
    return m_sections.contains(hashStr(sectionName));
}

template <typename T>
T IniFile::get(const std::string_view& sectionName, const std::string_view& propertyName, T defaultValue) const
{
    const auto sectionPair = m_sections.find(hashStr(sectionName));
    if (sectionPair != m_sections.end())
    {
        const auto propertyPair = sectionPair->second.properties.find(hashStr(propertyName));
        if (propertyPair != sectionPair->second.properties.end())
        {
            T value{};
            const auto result = std::from_chars(propertyPair->second.value.data(), 
                propertyPair->second.value.data() + propertyPair->second.value.size(), value);

            if (result.ec == std::errc{})
                return value;
        }
    }
    return defaultValue;
}

template<typename T>
inline void IniFile::enumerate(const T& function) const
{
    for (const auto& [_, section] : m_sections)
    {
        for (auto& property : section.properties)
            function(section.name, property.second.name, property.second.value);
    }
}

template <typename T>
void IniFile::enumerate(const std::string_view& sectionName, const T& function) const
{
    const auto sectionPair = m_sections.find(hashStr(sectionName));
    if (sectionPair != m_sections.end())
    {
        for (const auto& property : sectionPair->second.properties)
            function(property.second.name, property.second.value);
    }
}
