#include <fstream>
#include <map>
#include <ranges>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <optional>

struct Type {
    std::string type_name;
};

struct auto_sql {

    auto_sql() {
        traverse_directory("/Users/m1saka/CLionProjects/sql/data/general_data");
        traverse_directory("/Users/m1saka/CLionProjects/sql/data/special_data");

        std::ifstream ifs("/Users/m1saka/CLionProjects/sql/special_function", std::ios::in);
        std::string str_line;
        while (std::getline(ifs, str_line)) {
            std::stringstream ss{str_line};
            std::string name, arg;
            ss >> name;
            while (ss >> arg) {
                special_func_map[name].emplace_back(arg);
            }
        }
        ifs.close();
    }

    std::vector<std::string> get_data_vec(std::string &type_name) {
        std::string old_type_name = type_name;
        if (type_name.substr(0, 6) == "ARRAY_") {
            // 获得是什么类型的ARRAY，如ARRAY<INT>
            std::string sub_type = type_name.substr(6);
            type_name = "ARRAY<" + sub_type + ">";
        }
        return data_map[old_type_name];
    }

    template<typename F>
    void generate_sql_string(F &&func_name, std::vector<Type> &arg_types) {
        std::ofstream ofs("/Users/m1saka/CLionProjects/sql/sql",
                          std::ios::app);
        std::ofstream mysql_ofs("/Users/m1saka/CLionProjects/sql/mysql", std::ios::app);

        if (arg_types.empty()) {
            ofs << std::format("select {}();", func_name) << "\n";
            return;
        }

        // 第i个下标对应第vector存储第i个参数可能的值
        std::vector<std::vector<std::string>> vec{};

        auto new_arg_types = check_func_name(func_name);
        // todo 这里搞个optional来判断一下函数参数有无特殊要求？ ok
        for (auto &arg: new_arg_types.has_value() ? new_arg_types.value() : arg_types) {
            // todo 特殊参数处理一下 ok
            vec.push_back(get_data_vec(arg.type_name));
        }

        // dfs 获得参数排列组合
        static int index = 0;

        auto get_sql_arg_comb = [&](auto &&self, int deep, std::vector<std::string> data_set) -> void {
            if (deep == (int) arg_types.size()) {
                std::string sql_format_string{std::format("select {}(", func_name)};
                std::string mysql_format_string{std::format("select {}(", func_name)};

                for (int i = 0; i < (int) arg_types.size(); i++) {
                    append_sql(sql_format_string, data_set[i], arg_types[i].type_name, new_arg_types.has_value());
                    mysql_format_string += std::format("{}, ", data_set[i]);
                }
                // 删除末尾空格和逗号
                sql_format_string.pop_back();
                sql_format_string.pop_back();
                sql_format_string += ");";

                mysql_format_string.pop_back();
                mysql_format_string.pop_back();
                mysql_format_string += ");";
                ofs << index << " " << sql_format_string << "\n";
                mysql_ofs << index << " " << mysql_format_string << "\n";
                std::cout << sql_format_string << "\n";
                index++;
                return;
            }

            for (int i = 0; i < vec[deep].size(); i++) {
                auto tmp_data_set = data_set;
                tmp_data_set.push_back(vec[deep][i]);
                self(self, deep + 1, tmp_data_set);
            }
        };

        get_sql_arg_comb(get_sql_arg_comb, 0, {});
    }

//             arg11
//           /
//      arg1
//     /     \
//    /       arg12
//  arg
//    \      arg21
//     \   /
//     arg2
//        \
//          arg22

    template<typename... Args>
    void split(std::vector<std::vector<Type>> &arg_types_vec, Args &&... args) {
        std::size_t split_arg_cnt = sizeof...(args);
        std::size_t arg_types_vec_size = arg_types_vec.size();
        std::vector<std::string> split_args{std::forward<Args>(args)...};

        for (int i = 1; i < split_arg_cnt; i++) {
            // copy arg
            for (int j = 0; j < arg_types_vec_size; j++) {
                arg_types_vec.push_back(arg_types_vec[j]);
                arg_types_vec.back().emplace_back(split_args[i]);
            }
        }

        for (int i = 0; i < arg_types_vec_size; i++) {
            arg_types_vec[i].emplace_back(split_args.front());
        }
    }

    void parse_function() {
        std::ifstream ifs("/Users/m1saka/CLionProjects/sql/function",
                          std::ios::in);
        std::string s;
        while (std::getline(ifs, s)) {
            // 函数签名为 [[func_name1, func_name2, ... ] return_type [arg1, arg2, ...]]
            if (s.find("[[") != std::string::npos) {
                std::string cur;
                // vec[0] 存储 func_name , vec[1] 存储 func_arg
                std::vector<std::string> vec;
                for (int i = 1; i < (int) s.size() - 2; i++) {
                    if (s[i] == ']') {
                        vec.push_back(cur);
                        cur.clear();
                    }
                    cur += s[i];
                    if (s[i] == '[') {
                        cur.clear();
                    }
                }

                std::vector<std::string> func_name_vec;
                std::vector<std::string> func_args_vec;

                std::string split_string{", "};
                for (auto func_name: std::views::split(vec[0], split_string)) {
                    func_name_vec.emplace_back(std::ranges::begin(func_name) + 1,
                                               std::ranges::end(func_name) - 1);
                }
                for (auto func_arg: std::views::split(vec[1], split_string)) {
                    func_args_vec.emplace_back(std::ranges::begin(func_arg) + 1,
                                               std::ranges::end(func_arg) - 1);
                }


                for (auto func_name: func_name_vec) {
                    std::vector<std::vector<Type>> arg_types_vec(1);
                    for (const auto &arg_type: func_args_vec) {
                        // todo V3的几种类型交给生成sql去做
                        if (arg_type.substr(0, 13) == "ARRAY_DECIMAL") {
                            split(arg_types_vec, "ARRAY_DECIMALV2", "ARRAY_DECIMALV3(1,1)", "ARRAY_DECIMALV3(1,0)",
                                  "ARRAY_DECIMALV3(3,5)", "ARRAY_DECIMALV3(5,3)", "ARRAY_DECIMALV3(27,9)");
                        } else if (arg_type.substr(0, 8) == "DATETIME") {
                            split(arg_types_vec, "DATETIMEV1", "DATETIMEV2");
                        } else if (arg_type.substr(0, 4) == "DATE") {
                            split(arg_types_vec, "DATEV1", "DATEV2");
                        } else if (arg_type.substr(0, 7) == "DECIMAL") {
                            split(arg_types_vec, "DECIMALV2", "DECIMALV3");
                        } else if (arg_type == "...") {
                            int cur_size = (int) arg_types_vec.size();
                            for (int i = 0; i < cur_size; i++) {
                                auto tmp = arg_types_vec[i];
                                arg_types_vec[i].push_back(tmp.back());
                                arg_types_vec[i].push_back(tmp.back());
                                tmp.push_back(tmp.back());
                                arg_types_vec.push_back(std::move(tmp));
                            }
                        } else {
                            for (auto &arg_types: arg_types_vec) {
                                arg_types.emplace_back(arg_type);
                            }
                        }
                    }

                    for (auto &i: arg_types_vec) {
                        generate_sql_string(func_name, i);
                    }
                }
            }
        }
        ifs.close();
    }

public:
    static std::string parse_decimalv3_json(const std::string &json) {
        int scale, precision;
        double value;

        std::stringstream ss{json};
        char ch;
        std::string key;

        while (ss >> ch) {
            if (ch == '"') {
                std::getline(ss, key, '"');
                // 跳过 : 字符
                ss >> ch;
                if (key == "scale") {
                    ss >> scale;
                } else if (key == "precision") {
                    ss >> precision;
                } else if (key == "value") {
                    ss >> value;
                    break;
                }
            }
        }
        return std::format("cast({} as DECIMALV3({},{})), ", value, scale, precision);
    }

    static std::string parse_date_fucntion_v2_json(const std::string &type_name, const std::string &json) {
        int precision;
        std::string value;
        std::stringstream ss{json};
        char ch;
        std::string key;

        while (ss >> ch) {
            if (ch == '"') {
                std::getline(ss, key, '"');
                // 跳过 : 字符
                ss >> ch;
                if (key == "precision") {
                    ss >> precision;
                } else if (key == "value") {
                    for (auto c: std::string{std::istreambuf_iterator<char>(ss), std::istreambuf_iterator<char>()}) {
                        if (c == '{' || c == '}') continue;
                        value += c;
                    }
                    break;
                }
            }
        }
        return std::format("cast({} as {}({})), ", value, type_name, precision);
    }

    static void append_sql(std::string &sql, std::string &arg_data, std::string &type_name, bool is_special) {
        // todo 这里记得判断下，如果类型是DECIMALV3 解析下json
        // todo 如果是DATEV2类型记得 转化 0 - 7
        if (arg_data == "NULL") {
            sql += "NULL, ";
        } else if (type_name == "DECIMALV3") {
            sql += parse_decimalv3_json(arg_data);
        } else if (type_name == "DATETIMEV2" || type_name == "DATEV2") {
            sql += parse_date_fucntion_v2_json(type_name, arg_data);
        } else if (!is_special) {
            sql += std::format("cast({} as {}), ", arg_data, type_name);
        } else {
            sql += std::format("{}, ", arg_data);
        }
    }

    void read_file_data(const std::filesystem::path &file_path) {
        std::ifstream ifs(file_path, std::ios::in);
        std::string type_name = file_path.filename().string();
        // 去掉 .csv 后缀,只保留类型名
        type_name = type_name.substr(0, type_name.length() - 4);

        std::string line;
        while (std::getline(ifs, line)) {
            data_map[type_name].emplace_back(line);
        }
        ifs.close();
    }

    void traverse_directory(const std::filesystem::path &dir_path) {
        for (const auto &entry: std::filesystem::directory_iterator(dir_path)) {
            if (std::filesystem::is_regular_file(entry.status())) {
                read_file_data(entry.path());
            }
        }
    }

    std::optional<std::vector<Type>> check_func_name(const std::string &func_name) {
        // key: func_name value: func_arg
        if (special_func_map.contains(func_name)) {
            return special_func_map.at(func_name);
        } else {
            return std::nullopt;
        }
    }

    std::map<std::string, std::vector<std::string>> data_map{};
    std::map<std::string, std::vector<Type>> special_func_map{};
};

int main() {
    auto_sql s;
    s.parse_function();
}