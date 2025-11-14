#include "../include/util.h"
#include "../include/log.h"

namespace Tools
{
    std::optional<std::string> JsonTools::serialize(const Json::Value& value)
    {
        Json::StreamWriterBuilder builder;
        auto writer = builder.newStreamWriter();
        std::stringstream ss;
        writer->write(value, &ss);
        return ss.str();
    }

    std::optional<Json::Value> JsonTools::deserialize(const std::string& json)
    {
        Json::Reader reader;
        Json::Value value;
        if (!reader.parse(json, value))
        {
            ERR("Failed to parse JSON: {}", json);
            return std::nullopt;
        }
        return value;
    }
    bool FileTools::read_from_file(const std::string& file_path, std::string* content)
    {
        std::ifstream ifs(file_path, std::ios::in | std::ios::binary);
        if (!ifs.is_open())
        {
            ERR("Failed to open {}: {}", file_path, strerror(errno));
            return false;
        }

        size_t size = ifs.seekg(0, std::ios::end).tellg();
        ifs.seekg(0, std::ios::beg);

        content->resize(size);

        ifs.read(&(*content)[0], size);
        if(ifs.good() == false){
            ERR("Failed to read {}: {}", file_path, strerror(errno));
            return false;
        }

        return true;
    }

    bool FileTools::write_to_file(const std::string& file_path, const std::string& content)
    {
        std::ofstream ofs(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            ERR("Failed to open {}: {}", file_path, strerror(errno));
            return false;
        }

        ofs.write(content.data(), content.size());
        if(ofs.good() == false){
            ERR("Failed to write {}: {}", file_path, strerror(errno));
            return false;
        }

        return true;
    }

    std::vector<std::string> StringTools::split(const std::string& str, 
            const std::string& sep)
    {
        size_t pos = 0, idx = 0;
        std::vector<std::string> res;
        while ((pos = str.find(sep, pos)) != std::string::npos)
        {
            // 1,2,3,4,,保证空字符串不会添加进入res中
            if(pos != idx) res.push_back(str.substr(idx, pos - idx));
            idx = pos + sep.size();
            pos = idx;
        }
        res.push_back(str.substr(idx));
        return res;
    }
    bool StringTools::regex_match(const std::string& str, const std::string& pattern)
    {
        return std::regex_match(str, std::regex(pattern));
    }
    

    std::string RandomTools::generate_random_string(int length, RandomType type)
    {
        static const std::string ALPHA_STR = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static const std::string NUM_STR = "0123456789";
        static const std::string MIX_STR = ALPHA_STR + NUM_STR;
        static const int MIN_LENTH = 4;
        // 产生一个随机数
        // 然后根据随机数从相应的字符串中取出相应的字符
        const std::string* use_str = nullptr;
        switch (type)
        {
            case RandomType::ALPHA:
                use_str = &ALPHA_STR;
                break;
            case RandomType::NUM:
                use_str = &NUM_STR;
                break;
            case RandomType::MIX:
                use_str = &MIX_STR;
                break;
            default:
                break;
        }
        std::stringstream ssm;
        // 产生随机数种子
        std::random_device rd;
        auto generate(rd());
        length = length > MIN_LENTH ? length - MIN_LENTH: length;
        for(int i = 0; i < length; i++)
        {
            // 利用美森旋转算法产生随机数
            
            ssm << use_str->at(generate % use_str->size());
        }
        if(length <= MIN_LENTH) return ssm.str();


        static std::atomic<unsigned int> number_base{0};
        int num = number_base.fetch_add(1);
        
        ssm << std::setw(4) << num;
        return ssm.str();
    }
}