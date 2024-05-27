#pragma once

#include "pch.h"

class FMyModel
{
public:
	static std::shared_ptr<FMyModel> LoadModel(const std::string& InPath);
};

