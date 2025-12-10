/*************************************************
  * 描述：yaml节点类型封装
  *
  * File：yamlnode.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2025/9/2
  * Update：
  * ************************************************/
#ifndef COREXI_COMMON_PC_YAMLNODE_H
#define COREXI_COMMON_PC_YAMLNODE_H

#include <common/export.h>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace YAML
{
	class Node;
}

namespace Config
{
	// IMPL模式的自定义YAML节点类，对yaml-cpp的node类进行了封装，没有实际用途，仅作为一个载体隐藏yaml-cppnode
	class CXI_COMMON_EXPORT YamlNode
	{
		friend class YamlTool;

	private:
		class YamlNodePrivate;

	public:
		YamlNode();                     //空节点
		YamlNode(const YamlNode& other);// 拷贝
		YamlNode& operator=(const YamlNode& other);
		explicit YamlNode(const std::string& filePath);

		~YamlNode();

		/**
		 * 判断空节点
		 * @return
		 */
		bool isNull() const;

		/**
		 * 是否为标量节点
		 * @return
		 */
		bool isScalar() const;

		/**
		 * 节点是否为Map
		 * @return
		 */
		bool isMap() const;

		/**
		 * 节点是否为序列
		 * @return
		 */
		bool isSequence() const;

		/**
		 * 节点是否定义
		 * @return
		 */
		bool isDefined() const;

		// 获取节点
		// ConfigNode operator[](const std::string& key) const;
		//
		// ConfigNode operator[](size_t index) const;

		/**
		 * 获取节点下子节点个数
		 * @return
		 */
		size_t size() const;

	private:
		explicit YamlNode(const YamlNodePrivate& nodeImpl);

		std::shared_ptr<YamlNodePrivate> pImplNode();

		/**
		* @brief 检查文件是否存在
		* @param filePath 文件路径
		* @return 文件是否存在
		*/
		bool fileExists(const std::string& filePath);

	private:
		std::shared_ptr<YamlNodePrivate> m_pImplNode;
	};
}// namespace Config


#endif//COREXI_COMMON_PC_YAMLNODE_H
