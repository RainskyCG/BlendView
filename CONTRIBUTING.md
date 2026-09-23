# 参与 BlendView 开发

感谢你参与 BlendView。提交改动前，请先确认问题能够在受支持的 Unreal Engine 版本中复现，并尽量让修改保持小而明确。

## 开发环境

- Windows 64 位
- Unreal Engine 5.6、5.7 或 5.8
- Visual Studio 与目标 UE 版本要求的 C++ 工具链

将仓库放在测试项目的 `Plugins/BlendView`，然后编译项目的 Editor Target。

## 提交 Issue

请提供：

- Unreal Engine 的精确版本和构建号。
- 清晰的复现步骤、预期结果和实际结果。
- 能说明问题的日志、截图或短视频。
- 关闭 BlendView 后问题是否仍然存在。
- 涉及输入时，说明使用的鼠标、键盘布局和相关快捷键设置。

## 提交 Pull Request

- 不要提交 `Binaries`、`Intermediate`、`Saved` 或本地发布包。
- 保持 UE 5.6、5.7、5.8 的条件编译边界清晰。
- 输入修改必须保证按下、移动、抬起和取消路径的所有权完整。
- 修改共享行为时补充或更新自动化测试。
- 在目标 UE 版本中完成编译；涉及实时交互时，再启动编辑器进行验证。
- PR 描述中列出行为变化、验证版本和已运行的测试。

## 代码风格

遵循 Unreal Engine 的 C++ 命名和格式习惯，优先复用现有模块边界。避免与当前问题无关的大范围重构。

## 许可证

提交代码即表示你同意以本仓库的 MIT License 发布该贡献。
