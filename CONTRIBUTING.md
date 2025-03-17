# 贡献指南

感谢您考虑为Qt摄像头控制应用做出贡献！以下是一些指导方针，以帮助您开始。

## 如何贡献

1. **Fork仓库**：首先，在GitHub上fork这个仓库到您自己的账户。

2. **克隆仓库**：
   ```
   git clone https://github.com/YOUR_USERNAME/qt-camera-control.git
   cd qt-camera-control
   ```

3. **创建分支**：
   ```
   git checkout -b feature/your-feature-name
   ```
   或
   ```
   git checkout -b fix/your-bug-fix
   ```

4. **进行更改**：进行您的代码更改，确保遵循项目的代码风格。

5. **测试**：确保您的更改不会破坏现有功能，并添加适当的测试。

6. **提交更改**：
   ```
   git add .
   git commit -m "描述您的更改"
   ```

7. **推送到GitHub**：
   ```
   git push origin feature/your-feature-name
   ```

8. **创建Pull Request**：在GitHub上创建一个Pull Request，描述您的更改。

## 代码风格指南

- 使用4个空格进行缩进
- 类名使用驼峰命名法（例如：`CameraControl`）
- 函数和变量使用小驼峰命名法（例如：`updateCameraList`）
- 常量使用全大写加下划线（例如：`MAX_CAMERAS`）
- 添加适当的注释，特别是对于复杂的逻辑
- 遵循Qt的编码规范

## 提交Pull Request前的检查清单

- [ ] 代码遵循项目的代码风格
- [ ] 添加了必要的注释
- [ ] 所有测试都通过
- [ ] 更新了相关文档（如果适用）
- [ ] 提交信息清晰地描述了更改

## 报告Bug

如果您发现了Bug，请使用GitHub的Issues功能创建一个新的Issue，并使用Bug报告模板提供尽可能多的信息。

## 提出新功能

如果您有新功能的想法，请使用GitHub的Issues功能创建一个新的Issue，并使用功能请求模板描述您的想法。

## 行为准则

- 尊重所有贡献者
- 提供建设性的反馈
- 专注于改进代码和用户体验

感谢您的贡献！ 