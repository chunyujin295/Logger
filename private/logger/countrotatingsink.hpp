/*************************************************
  * 描述：按照日志条数进行滚动的日志sink
  *
  * File：countrotatingsink.hpp
  * Author：chenyujin@mozihealthcare.cn
  * Date：2025/11/18
  * Update：调整文件命名规则：首文件无数字后缀，第二个文件开始按 .1.log 递增
  ************************************************/
#ifndef COREXI_COMMON_PC_COUNTROTATINGSINK_HPP
#define COREXI_COMMON_PC_COUNTROTATINGSINK_HPP

#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <vector>

namespace CustomSink
{
    namespace fs = std::filesystem;

    static bool ends_with(const std::string& s, const std::string& suffix)
    {
        if (s.size() < suffix.size()) return false;
        return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
    }

    static bool starts_with(const std::string &s, const std::string &prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    template<typename Mutex>
    class count_rotating_file_mt : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        count_rotating_file_mt(const std::string& base_filename,
                               size_t max_count,
                               size_t max_files = 0,
                               bool rotate_on_open = false)
            : max_count_(max_count)
            , max_files_(max_files)
            , rotate_on_open_(rotate_on_open)
        {
            // -------------------------------
            // 自动拆分 stem 和 .log 扩展名
            // -------------------------------
            if (ends_with(base_filename, ".log"))
            {
                // 去掉 .log
                stem_ = base_filename.substr(0, base_filename.size() - 4);
            }
            else
            {
                // 用户没写 .log，我们自动加一份
                stem_ = base_filename;
            }
            extension_ = ".log";

            scan_existing_files();

            if (rotate_on_open_ && !files_.empty())
            {
                open_new_file();
            }
            else
            {
                if (files_.empty())
                {
                    current_index_ = 0;
                }
                else
                {
                    current_index_ = extract_index(files_.back());
                }
                open_current_file();
            }
        }

    protected:
    	void sink_it_(const spdlog::details::log_msg& msg) override
    	{
    		spdlog::memory_buf_t buf;
    		this->formatter_->format(msg, buf);

    		// ---------------------------
    		// 去掉尾部换行符（关键修复点）
    		// ---------------------------
    		size_t size = buf.size();
    		while (size > 0 && (buf[size - 1] == '\n' || buf[size - 1] == '\r'))
    		{
    			--size;
    		}

    		file_.write(buf.data(), size);
    		file_.put('\n');            // 我们自己写一个换行，避免重复
    		log_count_++;

    		if (log_count_ >= max_count_)
    		{
    			rotate_file();
    		}
    	}


        void flush_() override
        {
            file_.flush();
        }

    private:
        //-----------------------------------------------------
        // 文件结构：
        //   stem.log
        //   stem.1.log
        //   stem.2.log
        //-----------------------------------------------------
        void scan_existing_files()
        {
            for (auto& p : fs::directory_iterator("."))
            {
                if (!p.is_regular_file()) continue;

                const std::string name = p.path().filename().string();

                if (name == stem_ + extension_)
                {
                    files_.push_back(name);
                }
                else if (starts_with(name, stem_ + ".") && ends_with(name, extension_))
                {
                    files_.push_back(name);
                }
            }

            // 按 index 排序
            std::sort(files_.begin(), files_.end(), [&](const std::string& a, const std::string& b) {
                return extract_index(a) < extract_index(b);
            });
        }

        //-----------------------------------------------------
        // 提取序号：
        //   stem.log    -> 0
        //   stem.1.log  -> 1
        //-----------------------------------------------------
        size_t extract_index(const std::string& filename)
        {
            const std::string first = stem_ + extension_;
            if (filename == first) return 0;

            std::string prefix = stem_ + ".";
            if (starts_with(filename, prefix) && ends_with(filename, extension_))
            {
                size_t start = prefix.size();
                size_t end   = filename.size() - extension_.size();
                std::string middle = filename.substr(start, end - start);
                return std::stoull(middle);
            }

            return 0;
        }

        //-----------------------------------------------------
        // 生成文件名
        //-----------------------------------------------------
        std::string make_filename(size_t index) const
        {
            if (index == 0)
                return stem_ + extension_;
            return stem_ + "." + std::to_string(index) + extension_;
        }

        //-----------------------------------------------------
        // 打开当前文件
        //-----------------------------------------------------
        void open_current_file()
        {
            log_count_ = 0;

            std::string filename = make_filename(current_index_);
            file_.open(filename, std::ios::out | std::ios::app);

            if (files_.empty() || files_.back() != filename)
                files_.push_back(filename);
        }

        //-----------------------------------------------------
        // 新文件：index++
        //-----------------------------------------------------
        void open_new_file()
        {
            current_index_++;
            std::string filename = make_filename(current_index_);
            file_.open(filename, std::ios::out | std::ios::app);

            log_count_ = 0;
            files_.push_back(filename);

            cleanup_old_files();
        }

        void rotate_file()
        {
            file_.close();
            open_new_file();
        }

        void cleanup_old_files()
        {
            if (max_files_ == 0) return;

            while (files_.size() > max_files_)
            {
                fs::remove(files_.front());
                files_.erase(files_.begin());
            }
        }

    private:
        std::ofstream file_;

        std::string stem_;
        std::string extension_;

        size_t max_count_;
        size_t max_files_;
        bool rotate_on_open_;

        size_t log_count_ = 0;
        size_t current_index_ = 0;
        std::vector<std::string> files_;
    };
} // namespace CustomSink

#endif // COREXI_COMMON_PC_COUNTROTATINGSINK_HPP
