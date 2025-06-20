# Vulkan PBR 渲染器

一个基于 Vulkan API 构建的 3D 渲染器，实现了基础PBR、天空盒生成和实时性能监控功能。

## 特性

### 核心渲染
- **Vulkan API 集成** - 现代底层图形 API
- **PBR** - 使用金属/粗糙度的基础PBR材质
- **动态天空盒** - 程序化生成的渐变天空盒

### 资源加载
- **OBJ 模型加载** - 支持 OBJ 格式模型文件(仅基础加载)
- **纹理加载** - 使用 stb_image 支持 PNG 格式的纹理

### 相机与控制
- **FPS 风格相机** - 带鼠标视角的第一人称相机
- **移动控制** - FPS 的 WASD 移动方式
- **鼠标灵敏度** - 可配置的鼠标视角灵敏度
- **缩放控制** - 鼠标滚轮缩放功能

### 性能与界面
- **ImGui 集成** - 用于调试和性能指标的即时模式 GUI
- **GPU 信息显示** - 实时显示显卡信息
- **实时性能监控** - FPS 计数器和帧时间可视化
- **帧时间图表** - 渲染性能的可视化表示

### 着色器系统
- **GLSL 着色器编译** - 使用 shaderc 的运行时着色器编译
- **着色器热重载** - 从源代码动态加载着色器
- **多着色器阶段** - 支持顶点、片段和几何着色器

## 系统要求

### 必需依赖
- C++17 
- **CMake 3.16+**
- **Vulkan SDK 1.3+**
- **GLFW**
- **ImGui**
- **stb_image**

### 请参考CmakeList放置第三方库，自行修改CmakeList亦可

### 加载自定义模型
将 OBJ 文件放在 `models/` 目录中，并在 `main.cpp` 中修改模型路径：
```cpp
if (!engine.loadModel("models/your-model.obj")) {
    // 处理错误
}
```

### 加载自定义纹理
将纹理文件放在 `models/textures/` 目录中：
```cpp
if (!engine.loadTexture("models/textures/your-texture.png")) {
    // 处理错误
}
```
