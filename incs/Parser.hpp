#include <string>
#include <vector>


class Parser {
    private:
        std::vector<std::string>    tokens;
        size_t                      pos;
    
    private:
        std::string peek();
        std::string get();
        void expect(const std::string& token);

        void parseServer();
        void parseLocation();

    public:
        Parser();
        ~Parser();

        void loadFile(const std::string& path);
        void tokenize(const std::string& content);
        void parse();
};
