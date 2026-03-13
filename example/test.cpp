#include<iostream>
#include<string>
#include<vector>
#include<fstream>
#include<sys/stat.h>
#include"../source/server.hpp"
std::unordered_map<int, std::string> _statu_msg = {
    {100,  "Continue"},
    {101,  "Switching Protocol"},
    {102,  "Processing"},
    {103,  "Early Hints"},
    {200,  "OK"},
    {201,  "Created"},
    {202,  "Accepted"},
    {203,  "Non-Authoritative Information"},
    {204,  "No Content"},
    {205,  "Reset Content"},
    {206,  "Partial Content"},
    {207,  "Multi-Status"},
    {208,  "Already Reported"},
    {226,  "IM Used"},
    {300,  "Multiple Choice"},
    {301,  "Moved Permanently"},
    {302,  "Found"},
    {303,  "See Other"},
    {304,  "Not Modified"},
    {305,  "Use Proxy"},
    {306,  "unused"},
    {307,  "Temporary Redirect"},
    {308,  "Permanent Redirect"},
    {400,  "Bad Request"},
    {401,  "Unauthorized"},
    {402,  "Payment Required"},
    {403,  "Forbidden"},
    {404,  "Not Found"},
    {405,  "Method Not Allowed"},
    {406,  "Not Acceptable"},
    {407,  "Proxy Authentication Required"},
    {408,  "Request Timeout"},
    {409,  "Conflict"},
    {410,  "Gone"},
    {411,  "Length Required"},
    {412,  "Precondition Failed"},
    {413,  "Payload Too Large"},
    {414,  "URI Too Long"},
    {415,  "Unsupported Media Type"},
    {416,  "Range Not Satisfiable"},
    {417,  "Expectation Failed"},
    {418,  "I'm a teapot"},
    {421,  "Misdirected Request"},
    {422,  "Unprocessable Entity"},
    {423,  "Locked"},
    {424,  "Failed Dependency"},
    {425,  "Too Early"},
    {426,  "Upgrade Required"},
    {428,  "Precondition Required"},
    {429,  "Too Many Requests"},
    {431,  "Request Header Fields Too Large"},
    {451,  "Unavailable For Legal Reasons"},
    {501,  "Not Implemented"},
    {502,  "Bad Gateway"},
    {503,  "Service Unavailable"},
    {504,  "Gateway Timeout"},
    {505,  "HTTP Version Not Supported"},
    {506,  "Variant Also Negotiates"},
    {507,  "Insufficient Storage"},
    {508,  "Loop Detected"},
    {510,  "Not Extended"},
    {511,  "Network Authentication Required"}
};

std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};
static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry) {
    size_t offset = 0;
    // 有10个字符，offset是查找的起始位置，范围应该是0~9，offset==10就代表已经越界了
    while(offset < src.size()) {  //等于就越界了
        size_t pos = src.find(sep, offset);//在src字符串偏移量offset处，开始向后查找sep字符/字串，返回查找到的位置
        if (pos == std::string::npos) {//没有找到特定的字符
            //将剩余的部分当作一个字串，放入arry中
            if(pos == src.size()) break; //走到了字符串的末尾，直接跳出
            arry->push_back(src.substr(offset));
            return arry->size();
        }
        if (pos == offset) {
            offset = pos + sep.size();
            continue;//当前字串是一个空的，没有内容
        }
        arry->push_back(src.substr(offset, pos - offset));
        offset = pos + sep.size();
    }
    return arry->size();
}
//读取文件的所有内容，将读取的内容放到一个Buffer中
static bool ReadFile(const std::string &filename, Buffer *buf) {
    std::ifstream ifs(filename,std::ios::binary); //ifstream输入文件流类,std::ios::binary以二进制模式打开文件
    if (ifs.is_open() == false) {  //打开失败
        printf("OPEN %s FILE FAILED!!", filename.c_str());
        return false;
    }
    size_t fsize = 0;  //文件大小
    ifs.seekg(0, ifs.end);//跳转读写位置到末尾
    fsize = ifs.tellg();  //获取当前读写位置相对于起始位置的偏移量，从末尾偏移刚好就是文件大小
    ifs.seekg(0, ifs.beg);//跳转到起始位置
    
    // 确保Buffer有足够的空间
    buf->EnsureWriteSpace(fsize);
    
    // 读取文件内容到Buffer
    ifs.read(buf->Write_pos(), fsize);
    if (ifs.good() == false) {
        printf("READ %s FILE FAILED!!", filename.c_str());
        ifs.close();  //异常关闭文件流
        return false;
    }
    
    // 更新写偏移
    buf->MoveWriteOffset(fsize);
    
    ifs.close();  //正常关闭文件流
    return true;
}
static std::string UrlEncode(const std::string url, bool convert_space_to_plus) {
    std::string res;
    for (auto &c : url) {
        if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c)) {
            res += c;
            continue;
        }
        if (c == ' ' && convert_space_to_plus == true) {
            res += '+';
            continue;
        }
        //剩下的字符都是需要编码成为 %HH 格式
        char tmp[4] = {0};
        //snprintf 与 printf比较类似，都是格式化字符串，只不过一个是打印，一个是放到一块空间中
        snprintf(tmp, 4, "%%%02X", c);
        res += tmp;
    }
    return res;
}
static bool WriteFile(const std::string &filename, const std::string &buf) {
    std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
    if (ofs.is_open() == false) {
        printf("OPEN %s FILE FAILED!!", filename.c_str());
        return false;
    }
    ofs.write(buf.c_str(), buf.size());
    if (ofs.good() == false) {
        ERR_LOG("WRITE %s FILE FAILED!", filename.c_str());
        ofs.close();    
        return false;
    }
    ofs.close();
    return true;
}
static char HEXTOI(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }else if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }else if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return -1; 
}
static std::string UrlDecode(const std::string url, bool convert_plus_to_space) {
    //遇到了%，则将紧随其后的2个字符，转换为数字，第一个数字左移4位，然后加上第二个数字  + -> 2b  %2b->2 << 4 + 11
    std::string res;
    for (int i = 0; i < url.size(); i++) {
        if (url[i] == '+' && convert_plus_to_space == true) {
            res += ' ';
            continue;
        }
        if (url[i] == '%' && (i + 2) < url.size()) {
            char v1 = HEXTOI(url[i + 1]);
            char v2 = HEXTOI(url[i + 2]);
            char v = v1 * 16 + v2;
            res += v;
            i += 2;
            continue;
        }
        res += url[i];
    }
    return res;
}
//响应状态码的描述信息获取
static std::string StatuDesc(int statu) {
    
    auto it = _statu_msg.find(statu);
    if (it != _statu_msg.end()) {
        return it->second;
    }
    return "Unknow";
}
//根据文件后缀名获取文件mime
static std::string ExtMime(const std::string &filename) {
    
    // a.b.txt  先获取文件扩展名
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) {
        return "application/octet-stream";  //没有扩展名，默认是二进制流
    }
    //根据扩展名，获取mime
    std::string ext = filename.substr(pos);
    auto it = _mime_msg.find(ext);
    if (it == _mime_msg.end()) {
        return "application/octet-stream";
    }
    return it->second;
}
//判断一个文件是否是一个目录
static bool IsDirectory(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st); //获取文件属性
    if (ret < 0) {
        return false;
    }
    return S_ISDIR(st.st_mode); //判断是否是目录
}
//判断一个文件是否是一个普通文件
static bool IsRegular(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st); //获取文件属性
    if (ret < 0) {
        return false;
    }
    return S_ISREG(st.st_mode); //判断是否是普通文件
}
int main() {
    // 测试字符串分割函数
    // std::string str = "abc,,,,def,ghi";
    // std::vector<std::string> arry;
    // Split(str, ",", &arry);
    // for (auto &s : arry) {
    //     std::cout <<"["<< s <<"]"<< std::endl;
    // }

    // 测试读取文件函数
    // Buffer buf;
    // if (ReadFile("test.txt", &buf) == false) {
    //     printf("READ test.txt FILE FAILED!!");
    //     return -1;
    // }
    // std::cout<<buf.Read_pos()<<std::endl;
    // buf.MoveReadOffset(buf.ReadAbleSize());

    // // 测试URL编码函数
    // std::string str = "/index.html?user=hello+world";
    // std::string tmp = "C  ";
    // std::string res = UrlEncode(tmp, false);
    // std::cout<<res<<std::endl;
    // // 测试URL解码函数
    // res = UrlDecode(res, true);
    // std::cout<<res<<std::endl;

    // // 测试响应状态码描述信息获取
    // std::cout<<StatuDesc(404)<<std::endl;
    // // 测试根据文件后缀名获取文件mime
    // std::cout<<ExtMime("a.png")<<std::endl;
    
    // 测试判断一个文件是否是一个目录
    std::cout<<IsDirectory("test.txt")<<std::endl;
    // 测试判断一个文件是否是一个普通文件
    std::cout<<IsRegular("test.txt")<<std::endl;
} 