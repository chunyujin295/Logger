/*************************************************
  * 描述：
  *
  * File：anytostring.h
  * Author：3029795434@qq.com
  * Date：2026/2/11
  * Update：
  * ************************************************/
#ifndef LOGGER_ANYTOSTRING_H
#define LOGGER_ANYTOSTRING_H
#include <any>
#include <codecvt>
#include <optional>
#include <string>

namespace LoggerUtil
{
    inline std::string floatingNumToString(double value)
    {
        if (value == 0.0 || value == -0.0)
        {
            return "0";
        }

        std::ostringstream out;
        int prec = std::numeric_limits<double>::digits10;
        std::string res;
        double tmp = value;
        while (prec >= 7)
        {
            out.clear();
            out.str(std::string());
            out.precision(prec); //覆盖默认精度
            out << tmp;
            std::string str = out.str(); //从流中取出字符串
            res = str;
            tmp = std::stod(str);
            prec -= 1;
        }

        std::replace(res.begin(), res.end(), 'e', 'E');
        return res;
    }

    inline std::optional<std::string> anyToString(const char* fileName, int fileLine, const char* function,
                                                  const std::any& data)
    {
        if (!data.has_value())
        {
            return std::nullopt;
        }
        if (data.type() == typeid(nullptr))
        {
            return std::nullopt;
        }
        if (data.type() == typeid(std::string))
        {
            return std::any_cast<std::string>(data);
        }
        if (data.type() == typeid(std::string &))
        {
            return std::any_cast<std::string>(data);
        }
        if (data.type() == typeid(const std::string &))
        {
            return std::any_cast<std::string>(data);
        }
        if (data.type() == typeid(const char*)) // 添加对 const char* 的支持
        {
            return std::any_cast<const char*>(data); // 直接返回字符串
        }
        if (data.type() == typeid(long)) // 添加对 const char* 的支持
        {
            return std::to_string(std::any_cast<long>(data));
        }
        if (data.type() == typeid(long long)) // 添加对 const char* 的支持
        {
            return std::to_string(std::any_cast<long long>(data));
        }
        if (data.type() == typeid(char*))
        {
            return std::any_cast<char*>(data);
        }
        if (data.type() == typeid(int))
        {
            return std::to_string(std::any_cast<int>(data));
        }
        if (data.type() == typeid(unsigned int))
        {
            return std::to_string(std::any_cast<unsigned int>(data));
        }
        if (data.type() == typeid(float))
        {
            return std::to_string(std::any_cast<float>(data));
        }
        if (data.type() == typeid(double))
        {
            return floatingNumToString(std::any_cast<double>(data));
        }
        if (data.type() == typeid(std::wstring))
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
            std::string utf8String = converter.to_bytes(std::any_cast<std::wstring>(data));
            return utf8String;
        }
        if (data.type() == typeid(wchar_t*))
        {
            std::wstring wStr = std::any_cast<wchar_t*>(data);
            std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
            std::string utf8String = converter.to_bytes(wStr);
            return utf8String;
        }
        if (data.type() == typeid(const wchar_t*))
        {
            std::wstring wStr = std::any_cast<const wchar_t*>(data);
            std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
            std::string utf8String = converter.to_bytes(wStr);
            return utf8String;
        }
        if (data.type() == typeid(size_t))
        {
            return std::to_string(std::any_cast<size_t>(data));
        }
        if (data.type() == typeid(bool))
        {
            if (std::any_cast<bool>(data)) return "true";
            return "false";
        }
        if (data.type() == typeid(uint64_t))
        {
            return std::to_string(std::any_cast<uint64_t>(data));
        }
        // if (data.type() == typeid(QString))
        // {
        // 	return std::any_cast<QString>(data).toStdString();
        // }
        // if (data.type() == typeid(QColor))
        // {
        // 	QColor color = std::any_cast<QColor>(data);
        // 	return QString("rgba(%1, %2, %3,%4)")
        // 			.arg(color.red())
        // 			.arg(color.green())
        // 			.arg(color.blue())
        // 			.arg(color.alpha())
        // 			.toStdString();
        // 	;
        // }
        // if (data.type() == typeid(QStringList))
        // {
        // 	auto strList = std::any_cast<QStringList>(data).toStdList();
        // 	if (strList.empty())
        // 	{
        // 		return {};
        // 	}
        //
        // 	std::string str = "(";
        // 	for (QString i: strList)
        // 	{
        // 		str += "\"" + i.toStdString() + "\",";
        // 	}
        // 	str.replace(str.end() - 1, str.end(), "");
        // 	str += ")";
        // 	return str;
        // }
        // if (data.type() == typeid(QByteArray))
        // {
        // 	return std::any_cast<QByteArray>(data).toStdString();
        // }
        // if (data.type() == typeid(qint64))
        // {
        // 	return std::to_string(std::any_cast<qint64>(data));
        // }
        // if (data.type() == typeid(QPoint))
        // {
        // 	std::string str = "(" + std::to_string(std::any_cast<QPoint>(data).x()) + "," + std::to_string(std::any_cast<QPoint>(data).y()) + ")";
        // 	return str;
        // }
        // if (data.type() == typeid(QPointF))
        // {
        // 	std::string str = "(" + floatingNumToString(std::any_cast<QPointF>(data).x()) + "," + floatingNumToString(std::any_cast<QPointF>(data).y()) + ")";
        // 	return str;
        // }

        return std::nullopt;
    }
}

#endif //LOGGER_ANYTOSTRING_H
