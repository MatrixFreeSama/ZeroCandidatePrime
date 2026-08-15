# ZeroCandidatePrime

Windows 单文件程序。

## 当前模式

- **Fast Bootstrap**：保持 primecount / traditional fallback 自己最适合的 bootstrap 路线，不强行改写。
- **Self Bootstrap**：从 `(1,2)` 开始，每一个 `p_k -> p_(k+1)` 都把上一 successor 的 prime-dimension basis 丢弃，重新从 `q0=2` 按需生成 live dimensions。NVIDIA 可用时 256 CUDA lanes 并行投影这些 live dimensions。
- **Direct p**：Prime Gate 后直接进入同一 exact Matrix-Free successor core。每次 Direct 调用本来就是 fresh dimension state。
- **Record Experiment**：保留 128-bit bounded implicit-recursion 实验路径，用于观察固定 causal-depth 假设；种子后的输出仍为 `PROVISIONAL / UNVERIFIED`。

## Matrix-Free / implicit contract

- 不构造自然数候选区间。
- 不构造 candidate pool。
- 不构造 dense/global interaction matrix。
- 用户算法内部的 q 维度由 `q_(k+1)=q_k+M_k(q_k)` 按需生成。
- Self Bootstrap 不跨 prime successor 保留 q-basis。
- Direct p 只存在当前 successor 的 live dimension workspace。
- Tensor Core 不使用。

Fast Bootstrap 只是外部起点解析器，不要求它改用用户算法。用户自己的 successor / Self Bootstrap 才完整采用上述算法和 NVIDIA 路径。

## UI 与 q0 更新

- 所有主按钮改为按当前系统字体文本实测尺寸自适应，空间不足时自动换行，不再用固定按钮宽度硬塞标题。
- 用户自己的 CPU / NVIDIA 求解路径不再用 `M_0(x)=1+(x mod 2)` 的奇偶闭式快捷方式。`q0=2` 现在作为普通隐式维度走统一的命中/幸存递归。
- Fast Bootstrap / Prime Gate / Exact Pass 属于外部传统路径，仍可保留它们各自适合的奇偶优化，且不会反馈进用户生成器。
