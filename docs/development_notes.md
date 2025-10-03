# 开发者说明

## 第三方依赖与工具

- FFmpeg 预编译包、Dependencies 工具等大文件统一放置在仓库根目录（如 `ffmpeg-master-shared/`）。
- `.gitignore` 已忽略上述目录及压缩包，如需更新请在提交信息或文档中标注来源与版本。
- 推荐在 `third_party/` 内保留必要的 README，以说明如何重新获取外部依赖。

## 配置与模型

- 参考 `docs/model_configuration.md`，检测模型使用 `det:` 命名空间，分割任务保留占位以便未来扩展。
- 若环境中暂缺分割模型，可保持占位配置不变，后端将给出清晰的错误提示。

## REST 文档

- `docs/api_endpoints.md` 汇总当前接口；原 `/api/analysis/*` 等旧端点已废弃。

如需新增功能，请在对应文档中更新说明，保持仓库结构清晰。
