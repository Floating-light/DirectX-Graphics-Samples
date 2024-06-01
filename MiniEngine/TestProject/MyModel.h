#pragma once

#include "pch.h"
#include "ModelLoader.h"

class FMyModel
{
public:
	static std::shared_ptr<Renderer::ModelData> LoadModel(const std::string& InPath); 
};

