/*************************************************
  * 描述：按日期建立目录 + 当天按大小(size)滚动的日志sink（懒创建版本）
  *       - 入参：父目录(root_dir) + 文件前缀(stem)
  *       - 每天目录：root/YYYY-MM-DD/
  *       - 文件命名：stem.log, stem.1.log, stem.2.log ...（覆盖式滚动，rename链条）
  *       - 懒创建：构造/跨天/轮转不主动创建新文件，首次写入才创建，避免空文件
  *
  * File：daily_dir_size_rotating_file_sink.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/1/30
  ************************************************/
#ifndef COREXI_COMMON_PC_DAILY_DIR_SIZE_ROTATING_FILE_SINK_H
#define COREXI_COMMON_PC_DAILY_DIR_SIZE_ROTATING_FILE_SINK_H

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <vector>

namespace CustomSink
{
	namespace fs = std::filesystem;

	// static bool ends_with(const std::string& s, const std::string& suffix)
	// {
	// 	if (s.size() < suffix.size()) return false;
	// 	return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
	// }
	//
	// static bool starts_with(const std::string& s, const std::string& prefix)
	// {
	// 	return s.rfind(prefix, 0) == 0;
	// }

	// 取本地日期：YYYY-MM-DD
	static std::string local_date_yyyy_mm_dd()
	{
		std::time_t t = std::time(nullptr);
		std::tm tmv{};
#if defined(_WIN32)
		localtime_s(&tmv, &t);
#else
		localtime_r(&t, &tmv);
#endif
		char buf[11];
		std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
					  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
		return std::string(buf);
	}

	template<typename Mutex>
	class daily_dir_size_rotating_file_mt : public spdlog::sinks::base_sink<Mutex>
	{
	public:
		// root_dir: 父目录（例如 "logs"）
		// stem: 文件前缀（例如 "app"）
		// 输出：logs/YYYY-MM-DD/app.log, app.1.log, ...
		// max_files: 备份文件最大数量（1..N），0表示不保留备份（等价不滚动或每次删掉旧？这里按spdlog：0仍可滚动但只保留stem.log）
		daily_dir_size_rotating_file_mt(const std::string& root_dir,
										const std::string& stem,
										size_t max_size_bytes,
										size_t max_files = 5,
										bool rotate_on_open = false)
			: root_dir_(fs::path(root_dir)),
			  stem_(stem),
			  extension_(".log"),
			  max_size_(max_size_bytes),
			  max_files_(max_files),
			  rotate_on_open_(rotate_on_open)
		{
			current_date_ = local_date_yyyy_mm_dd();
			opened_ = false;
			current_size_ = 0;
			rotated_on_open_done_ = false;

			// 懒创建：只确保目录存在（你也可以选择连目录都懒创建；这里保留你原习惯：目录先建）
			ensure_day_dir_();
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			maybe_roll_day_();

			spdlog::memory_buf_t buf;
			this->formatter_->format(msg, buf);

			// 去掉尾部换行符
			size_t size = buf.size();
			while (size > 0 && (buf[size - 1] == '\n' || buf[size - 1] == '\r'))
				--size;

			const size_t incoming = size + 1; // + '\n'

			// rotate_on_open：第一次真正写入前，如果存在旧stem.log则先滚动（但仍不创建新文件）
			if (!rotated_on_open_done_)
			{
				if (rotate_on_open_)
				{
					const auto base_sz = probe_file_size_noopen_(base_path_());
					if (base_sz > 0)
					{
						rotate_files_(); // rename链条
					}
				}
				rotated_on_open_done_ = true;
			}

			// 写入前判断是否需要滚动（严格保证写入后不超限）
			if (max_size_ > 0)
			{
				size_t base_size = 0;
				if (opened_ && file_.is_open())
				{
					base_size = current_size_;
				}
				else
				{
					base_size = probe_file_size_noopen_(base_path_());
					current_size_ = base_size;
				}

				if (base_size + incoming > max_size_)
				{
					rotate_files_(); // 关闭 + rename链条（覆盖式）
				}
			}

			// 确保打开 stem.log（懒打开：只有到这里才真正创建文件）
			ensure_opened_for_write_();

			file_.write(buf.data(), static_cast<std::streamsize>(size));
			file_.put('\n');
			current_size_ += incoming;
		}

		void flush_() override
		{
			if (file_.is_open())
				file_.flush();
		}

	private:
		// root/YYYY-MM-DD
		fs::path day_dir_() const
		{
			return root_dir_ / fs::path(current_date_);
		}

		void ensure_day_dir_()
		{
			std::error_code ec;
			fs::create_directories(day_dir_(), ec);
		}

		// stem.log
		std::string base_filename_() const
		{
			return stem_ + extension_;
		}

		// stem.N.log
		std::string rotated_filename_(size_t index) const
		{
			return stem_ + "." + std::to_string(index) + extension_;
		}

		fs::path base_path_() const
		{
			return day_dir_() / fs::path(base_filename_());
		}

		fs::path rotated_path_(size_t index) const
		{
			return day_dir_() / fs::path(rotated_filename_(index));
		}

		size_t probe_file_size_noopen_(const fs::path& path) const
		{
			std::error_code ec;
			if (!fs::exists(path, ec) || ec) return 0;
			auto sz = fs::file_size(path, ec);
			if (ec) return 0;
			return static_cast<size_t>(sz);
		}

		void ensure_opened_for_write_()
		{
			if (opened_ && file_.is_open())
				return;

			ensure_day_dir_(); // 防御：目录可能被删
			const fs::path p = base_path_();
			file_.open(p.string(), std::ios::out | std::ios::app);

			// 续写：size 从现有文件大小开始
			current_size_ = probe_file_size_noopen_(p);
			opened_ = true;
		}

		// 覆盖式滚动（spdlog常见行为）：
		// 1) 关闭当前文件（若打开）
		// 2) 删除最老 stem.max_files.log
		// 3) i = max_files-1..1: stem.i.log -> stem.(i+1).log
		// 4) stem.log -> stem.1.log
		// 5) 不创建新stem.log（懒创建，下一次写入再创建/打开）
		void rotate_files_()
		{
			if (file_.is_open())
			{
				file_.flush();
				file_.close();
			}
			opened_ = false;
			current_size_ = 0;

			ensure_day_dir_(); // 防御

			// max_files_ == 0：按常见语义“不保留备份”，那就直接清空stem.log（这里选择：把stem.log删掉，相当于“滚动但无备份”）
			// 如果你更想“不滚动”，可以把这段改成 return;
			if (max_files_ == 0)
			{
				std::error_code ec;
				fs::remove(base_path_(), ec);
				return;
			}

			std::error_code ec;

			// 删除最老的
			fs::remove(rotated_path_(max_files_), ec);

			// 依次搬运：N-1 -> N ... 1 -> 2
			for (size_t i = max_files_ - 1; i >= 1; --i)
			{
				fs::path src = rotated_path_(i);
				fs::path dst = rotated_path_(i + 1);
				if (fs::exists(src, ec) && !ec)
				{
					// 先删目标，避免 Windows rename 失败
					fs::remove(dst, ec);
					ec.clear();
					fs::rename(src, dst, ec);
				}

				// size_t 无符号，i==1后再--会溢出，手动break
				if (i == 1) break;
			}

			// stem.log -> stem.1.log
			{
				fs::path src = base_path_();
				fs::path dst = rotated_path_(1);
				if (fs::exists(src, ec) && !ec)
				{
					fs::remove(dst, ec);
					ec.clear();
					fs::rename(src, dst, ec);
				}
			}

			// 注意：这里不打开新stem.log，保持懒创建
		}

		// 跨天：只更新日期/状态，不创建文件（懒创建）
		void maybe_roll_day_()
		{
			const std::string today = local_date_yyyy_mm_dd();
			if (today == current_date_)
				return;

			if (file_.is_open())
			{
				file_.flush();
				file_.close();
			}

			opened_ = false;
			current_size_ = 0;
			current_date_ = today;

			// 新的一天：rotate_on_open 重新生效
			rotated_on_open_done_ = false;

			ensure_day_dir_(); // 只建目录，不建文件
		}

	private:
		std::ofstream file_;

		fs::path root_dir_;
		std::string stem_;
		std::string extension_;

		size_t max_size_;
		size_t max_files_;
		bool rotate_on_open_;

		std::string current_date_;

		bool opened_ = false;
		size_t current_size_ = 0;

		bool rotated_on_open_done_ = false; // 每天第一次写入前的 rotate_on_open 只做一次
	};

} // namespace CustomSink

#endif // COREXI_COMMON_PC_DAILY_DIR_SIZE_ROTATING_FILE_SINK_H
