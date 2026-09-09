# WipePDF (清印 PDF) - C++ 原生重构版

> 高性能、原生 C++ (MSVC) + Qt 6 架构的 PDF 顽固水印与超链接清除工具。

---

## 🌟 重构亮点与对比

原 Python + Tkinter 版本在实际商业与高频使用中存在体积臃肿、启动缓慢、多线程性能瓶颈与界面老旧等痛点。全新 WipePDF 采用 **原生 C++ (MSVC 2022) + Qt 6.5.3 + MuPDF C API** 彻底重构：

| 指标 / 特性 | 原 Python 版本 | WipePDF C++ 重构版 |
|---|---|---|
| **主程序体积** | 约 60 ~ 100 MB (PyInstaller 打包) | **仅 218 KB** (`WipePDF.exe`) |
| **启动速度** | 需释放临时目录，冷启动 3~5 秒 | **原生编译，零延迟秒开** |
| **并发与多线程** | 受限于 Python GIL 锁 | **原生 C++ 线程池 (`QtConcurrent`)，多核满载并行** |
| **反编译防护** | Python 字节码极易被逆向与破解 | **原生机器码 (x64 Native)，商业级安全防护** |
| **界面与交互** | Tkinter 原生控件，无抗锯齿高亮 | **现代暗黑主题 Qt 6 界面，平滑滚轮缩放与交互** |
| **交互撤销 (Undo)** | 不支持单步撤销，误触需全部清空 | **支持单步撤销 (`Undo` / 快捷键 `Ctrl+Z`)** |
| **高亮与滚动渲染** | 滚轮滚动容易导致红框消失或残影 | **状态数据与视图彻底解耦，缩放滚动 100% 同步** |
| **文件锁与覆盖写入** | Windows 下覆盖写可能导致文件锁冲突死锁 | **安全原子写入 (`.tmp` + 显式解开只读预览锁)** |
| **国际化 (i18n)** | 需适配不同字典 | **原生中英双语支持（简中「清印 PDF」/ 英文「WipePDF」）** |

---

## 🛠 架构设计

```
d:/AI/AIProjects/WipePDF/
├── build.ps1                   # 一键自动化构建脚本 (MSVC + Ninja + Qt6)
├── CMakeLists.txt              # CMake 现代构建定义
├── dist/                       # 绿色免安装发布产物 (含 WipePDF.exe 与动态库)
├── src/
│   ├── main.cpp                # 程序入口 (高分屏适配、主题应用)
│   ├── core/                   # 核心算法层 (与 UI 完全解耦，支持无头自动化)
│   │   ├── Element.h           # 水印与元素统一模型定义
│   │   ├── PdfDocument.h/.cpp  # MuPDF 底层封装 (文档生命周期、文本/链接提取、渲染、Redact)
│   │   ├── Detectors.h/.cpp    # 元素检测器 (链接正则、底部高度、文本正则、透明覆盖层)
│   │   ├── Matcher.h/.cpp      # 交互式点选跨页特征规则匹配引擎
│   │   ├── ContentEdit.h/.cpp  # 内容流语法级 Tokenizer 与重写引擎 (BT..ET / re..f)
│   │   └── WatermarkCleaner.h/.cpp # 批量与单文件去水印任务流水线
│   ├── ui/                     # 现代 Qt 6 UI 层
│   │   ├── MainWindow.h/.cpp   # 主窗口、侧边配置栏、多任务并发控制看板
│   │   └── PdfViewer.h/.cpp    # 交互式预览画布 (高精度点选拾取、平滑漫游缩放、持久红框高亮)
│   └── i18n/                   # 双语国际化
│       └── I18n.h/.cpp         # 运行时一键无缝双语切换
└── third_party/
    └── mupdf/                  # MuPDF x64 MSVC 预编译库与头文件
```

---

## 🚀 编译构建

### 环境要求
- Windows 10 / 11 (x64)
- Visual Studio 2022 Build Tools (MSVC v143+)
- Qt 6.5.3 (MSVC 2019/2022 64-bit)
- CMake 3.20+ & Ninja

### 一键构建命令
在 PowerShell 中直接运行项目根目录下的自动化构建脚本：
```powershell
.\build.ps1
```
如需全新干净重构，添加 `-Clean` 参数：
```powershell
.\build.ps1 -Clean
```

---

## 🧪 自动化测试套件

构建完成后，运行验证测试套件 `build\test_core.exe`：
```powershell
.\build\test_core.exe
```

**测试项目覆盖**：
1. PDF 页面加载与属性提取
2. 链接注释高精度检测
3. 文本块结构化提取与坐标转换
4. 页面底部特征区域裁剪
5. 交互式点选坐标逆投影与命中拾取
6. 跨页特征匹配规则自动生成与应用
7. 自动模式全量水印清除流水线
8. 单步撤销 (`Undo` / `Ctrl+Z`) 逻辑验证
9. 安全原子覆盖写入 (`Safe Overwrite`) 验证（无文件锁冲突）
10. 中英双语运行时热切换验证

---

## 📦 独立运行分发

编译后的免安装便携版位于 `dist/` 目录：
- 双击直接运行 `dist\WipePDF.exe` 即可使用，无需安装 Python 或任何外部运行时环境。
