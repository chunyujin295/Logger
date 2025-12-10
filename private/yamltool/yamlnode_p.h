/*************************************************
  * 描述：
  *
  * File：yamlnodepimpl.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2025/9/2
  * Update：
  * ************************************************/
#ifndef COREXI_COMMON_PC_YAMLNODEPIMPL_H
#define COREXI_COMMON_PC_YAMLNODEPIMPL_H

#include <common/yamltool/yamlnode.h>
#include <yaml-cpp/yaml.h>

namespace Config
{
	class YamlNode::YamlNodePrivate
	{
		friend class YamlNode;

	public:
		YamlNodePrivate();
		explicit YamlNodePrivate(const YAML::Node& node);

		YAML::Node& node();

	private:
		YAML::Node m_node;
	};
}// namespace Config

#endif//COREXI_COMMON_PC_YAMLNODEPIMPL_H
