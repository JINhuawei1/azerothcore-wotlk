/*
 * 飞升系统 - loader.h
 * 模块加载器
 */

#ifndef _MOD_ASCENSION_SYSTEM_LOADER_H_
#define _MOD_ASCENSION_SYSTEM_LOADER_H_

// 声明脚本加载函数
void AddAscensionSystemScripts();

// 模块加载入口
void AddAscensionSystemModuleScripts()
{
    AddAscensionSystemScripts();
}

#endif // _MOD_ASCENSION_SYSTEM_LOADER_H_
