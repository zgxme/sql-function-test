#include <fstream>
#include <map>
#include <ranges>
#include <sstream>
#include <iostream>

struct Type {
    std::string type_name;
};

auto get_data_vec(std::string &type_name,
                  std::map<std::string, std::vector<std::string>> &mp) -> std::vector<std::string> {
    if (type_name.substr(0, 6) == "ARRAY_") {
        // 获得是什么类型的ARRAY，如ARRAY<INT>
        std::string sub_type = type_name.substr(6);
        type_name = "ARRAY<" + sub_type + ">";
        // 将对应类型的全部数据拿出拼接成一个ARRAY,如 '[1, 2, 3]'
        // DECIMALV3有多个类型，如DECIMALV3(5,3), DECIMALV3(3,5), 都从DECIMALV3行中拿数据
        return mp[type_name];
    } else { // 非ARRAY类型
        // 多个DECIMALV3 类型,比如DECIMALV3(2,2) 都用同样的DECIMALV3 数据
        if (type_name.substr(0, 9) == "DECIMALV3") {
            return mp["DECIMALV3"];
        } else {
            return mp[type_name];
        }
    }
}


std::vector<Type> check_func_name(const std::string &func_name, std::vector<Type> &arg_types) {
    // key: func_name value: func_arg
    static std::map<std::string, std::vector<Type>> mp;
    static bool flag = false;

    if (!flag) {
        std::ifstream ifs("/Users/m1saka/CLionProjects/sql/special_function", std::ios::in);
        std::string str_line;
        while (std::getline(ifs, str_line)) {
            std::stringstream ss{str_line};
            std::string name, arg;
            ss >> name;
            while (ss >> arg) {
                mp[name].emplace_back(arg);
            }
        }
        ifs.close();
    }

    flag ^= true;

    if (mp.contains(func_name)) {
        return mp.at(func_name);
    } else {
        return arg_types;
    }
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


    // 存 参数类型 ‘string’ 对应第值
    std::map<std::string, std::vector<std::string>> mp;
    std::ifstream ifs("/Users/m1saka/CLionProjects/sql/data",
                      std::ios::in);

    std::string s;

    while (std::getline(ifs, s)) {
        std::stringstream ss(s);
        std::string type_name, data_string;
        ss >> type_name;
        mp[type_name].emplace_back("NULL");
        while (ss >> data_string) {
            if (type_name == "INTERVAL") {
                for (auto &ch: data_string) {
                    if (ch == '_') ch = ' ';
                }
            }
            mp[type_name].push_back(data_string);
        }
        if (type_name == "STRING" || type_name == "VARCAHR") {
            mp[type_name].emplace_back("aa bb cc");
        }
    }

    ifs.close();

    // 第i个下标对应第vector存储第i个参数可能的值
    std::vector<std::vector<std::string>> vec{};

    auto new_arg_types = check_func_name(func_name, arg_types);
    vec.reserve(new_arg_types.size());
    for (auto &arg: new_arg_types) {
        vec.push_back(get_data_vec(arg.type_name, mp));
    }

    // dfs 获得参数排列组合

    auto get_sql_arg_comb = [&](auto &&self, int deep, std::vector<std::string> data_set) -> void {
        if (deep == (int) arg_types.size()) {
            std::string sql_format_string{std::format("select {}(", func_name)};
            std::string mysql_format_string{std::format("select {}(", func_name)};

            for (int i = 0; i < (int) arg_types.size(); i++) {
                if (data_set[i] == "NULL") {
                    sql_format_string += "NULL, ";
                } else if (new_arg_types[i].type_name == arg_types[i].type_name) {
                    sql_format_string +=
                            std::format("cast({} as {}), ", data_set[i], arg_types[i].type_name);
                } else {
                    sql_format_string += std::format("{}, ", data_set[i]);
                }
                mysql_format_string += std::format("{}, ", data_set[i]);
            }
            // 删除末尾空格和逗号
            sql_format_string.pop_back();
            sql_format_string.pop_back();
            sql_format_string += ");";

            mysql_format_string.pop_back();
            mysql_format_string.pop_back();
            mysql_format_string += ");";
            ofs << sql_format_string << "\n";
            mysql_ofs << mysql_format_string << "\n";
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
                    if (arg_type.substr(0, 13) == "ARRAY_DECIMAL") {
                        split(arg_types_vec, "ARRAY_DECIMALV2", "ARRAY_DECIMALV3(1,1)", "ARRAY_DECIMALV3(1,0)",
                              "ARRAY_DECIMALV3(3,5)", "ARRAY_DECIMALV3(5,3)", "ARRAY_DECIMALV3(27,9)");
                    } else if (arg_type.substr(0, 8) == "DATETIME") {
                        split(arg_types_vec, "DATETIMEV1", "DATETIMEV2");
                    } else if (arg_type.substr(0, 4) == "DATE") {
                        split(arg_types_vec, "DATEV1", "DATEV2");
                    } else if (arg_type.substr(0, 7) == "DECIMAL") {
                        split(arg_types_vec, "DECIMALV2", "DECIMALV3(1,1)", "DECIMALV3(1,0)",
                              "DECIMALV3(3,5)", "DECIMALV3(5,3)", "DECIMALV3(27,9)");
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

int main() {
    parse_function();
}