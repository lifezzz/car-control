# 四轮小车控制模拟（C11）

用标准 C 实现的四轮小车控制模拟程序，从标准输入逐行读取命令，维护小车状态、自动/手动控制要求、目标运动状态与四个车轮的实际速度。

- 标准 C（C11），**不依赖任何第三方库**；
- 分层结构：运动描述表 / 命令解析 / 小车逻辑 / 文本界面，互不耦合；
- `status` 严格按题目规定输出 6 行，且 **stdout 只包含 status 输出块**（其他提示全部写 stderr），方便自动化比对；
- 自带 13 组回归用例与测试脚本（`tests/`）；
- 附带题目要求的 `REVIEW.md`。

## 构建与运行

```bash
make                # 生成 ./car（Windows 下为 car.exe）
./car < test.txt    # 按题目例子的方式运行（test.txt 是演示脚本）
./car               # 交互模式，逐行输入命令，输入 quit 或 Ctrl-D/Z 退出
```

也可以使用 CMake（Windows + MSVC 推荐走这条路，因为 `-MMD/-MP` 是 GCC/Clang 的参数）：

```bash
cmake -S . -B build
cmake --build build
./build/car < test.txt
```

清理与测试：

```bash
make clean
make test                       # 等价于 python3 tests/run_tests.py
```

## 命令

| 命令 | 说明 |
| --- | --- |
| `start` | 待机 → 运行（重复执行只提示，不报错） |
| `auto <motion> <speed>` | 保存自动控制要求 |
| `manual <motion> <speed>` | 保存手动控制要求（并取得控制权） |
| `auto stop` / `manual stop` | 保存"停止"要求（不带速度参数） |
| `manual release` | 释放手动控制，回到自动控制 |
| `step` | 推进一次实际速度（每个轮子最多变化 20） |
| `emergency` | 立即急停：四轮实际速度直接变 0，状态 → EMERGENCY |
| `reset` | 复位：状态 → IDLE，清空自动/手动要求，四轮实际速度为 0 |
| `status` | 输出完整状态（固定 6 行） |
| `quit` / `exit` | 正常退出 |

`<motion>` 取 `forward` / `backward` / `left` / `right` / `stop`，`<speed>` 取 `0 ~ 100`。命令关键字大小写不敏感。

四轮顺序统一为：**左前轮 右前轮 左后轮 右后轮**。

| 运动方式 | 左前 | 右前 | 左后 | 右后 |
| --- | ---: | ---: | ---: | ---: |
| `forward` | + | + | + | + |
| `backward` | − | − | − | − |
| `left` | − | + | + | − |
| `right` | + | − | − | + |
| `stop` | 0 | 0 | 0 | 0 |

## 输出

`status` 连续输出以下 6 行，行间不夹杂任何其他文字：

```text
STATE RUNNING
CONTROL MANUAL
AUTO FORWARD 60
MANUAL LEFT 30
TARGET LEFT 30
WHEELS -20 20 20 -20
```

- `STATE`：`IDLE` / `RUNNING` / `EMERGENCY`
- `CONTROL`：当前生效的控制来源，`NONE` / `AUTO` / `MANUAL`
- `AUTO` / `MANUAL`：保存的要求；没有时输出 `NONE`；`stop` 输出 `STOP`
- `TARGET`：真正生效的运动要求；没有任何要求时输出 `STOP`
- `WHEELS`：左前、右前、左后、右后的实际速度

**流约定**：stdout 只有 status 块，其余 `[ok] / [error] / [warn] / [info]` 提示与交互式 banner 都写 stderr。
这样 `./car < test.txt > out.txt` 得到的就是可以直接与期望结果对比的文件，终端里依然能看到全部提示。

## 状态与规则

```text
             start
   IDLE  ------------->  RUNNING
     ^                      |
     |                      | emergency（任何状态都可用）
     | reset                v
     +-----------------  EMERGENCY  <-- 被锁存，只能由 reset 解除
```

- 普通运动命令与 `step` **只在 RUNNING 状态下有效**；
- 手动控制优先级高于自动控制，且手控生效期间仍可更新自动要求；
- 运动命令只改变**目标**速度，实际速度只由 `step` 逐步逼近（每个轮子每次最多变化 20，反向时正确经过 0）；
- `emergency` 直接清零实际速度（不经 `step`），`reset` 清空全部要求并回到待机。

## 边界与设计约定

题目未明确的地方，本实现按以下约定处理，并在 `REVIEW.md` 中做了说明：

1. **`start` 只在 IDLE 下有效**：RUNNING 下重复 `start` 只给出提示；EMERGENCY 下必须先 `reset`（急停是锁存状态，`reset` 是唯一的出口）。
2. **`emergency` 只清零四轮实际速度，并锁存状态**，不移动/清空已保存的 auto / manual 要求（按题目描述，清空要求是 `reset` 的职责）。因此急停后 `status` 仍会显示已保存的要求，而 `WHEELS` 恒为 `0 0 0 0`。
3. **速度 0 合法**（`0 ~ 100` 闭区间），`stop` 不带速度参数，`stop` 后面再跟参数视为多余参数。
4. **任何非法输入都只打印提示，不改变状态、不退出程序**：待机/急停下的运动命令、速度越界或非整数、缺参数、多参数、无法识别的命令、无手控时的 `manual release`、重复 `start`、超长行等。
5. `manual release` 在任何状态下都可用；没有手控时只给 `[warn]` 提示。

## 项目结构

```text
four-wheel-car-sim/
├── Makefile                构建脚本（make / make test / make clean）
├── CMakeLists.txt          CMake 构建（含 ctest 集成）
├── .gitignore
├── test.txt                演示脚本：./car < test.txt
├── README.md
├── REVIEW.md               题目要求的 Review 文档
├── include/
│   ├── config.h            速度范围、step 步长、缓冲区大小等常量
│   ├── motion.h            运动方式枚举、轮位枚举、运动描述表接口
│   ├── command.h           命令类型与解析结果
│   ├── car.h               小车状态、控制要求、对外操作接口
│   └── cli.h              文本界面接口（含流约定说明）
├── src/
│   ├── main.c              入口：初始化 + 交给 cli_run
│   ├── motion.c            运动方式表 + 目标速度展开
│   ├── command.c           分词与命令解析（只解析，不改状态）
│   ├── car.c               状态机、控制仲裁、四轮速度动力学
│   └── cli.c               读行、分发、status 打印、错误提示
└── tests/
    ├── run_tests.py        回归测试脚本（纯标准库，跨平台）
    └── cases/              13 组 .in / .out 用例
```

## 测试

```bash
make test
```

脚本对 `tests/cases/*.in` 逐个运行程序并比对 **stdout**（因此每个用例的 `.out` 就是若干个 status 块的拼接），同时校验：

- 进程退出码为 0；
- stdout 行数必须是 6 的倍数，且每 6 行的字段顺序为 `STATE / CONTROL / AUTO / MANUAL / TARGET / WHEELS`（保证 status 内不夹杂提示文字）。

覆盖的场景包括：题目示例、自动/手动切换与释放、连续 `step` 收敛、反向经过 0、急停后的锁存与 `reset`、状态门禁、速度越界与各类非法输入、大小写混写、`quit` 提前退出、文件末尾无换行等。

## 扩展指引

新增一种**运动方式**：在 `include/motion.h` 的 `motion_id_t` 中加入枚举值，并在 `src/motion.c` 的 `g_motions` 表里加一行（名称、输入词、四轮方向、是否需要速度）。解析、`status` 输出、四轮目标计算都会自动适配，`car.c` / `cli.c` 无需改动。

新增一种**控制方式**或需要"每轮速度不同"的复杂运动：详见 `REVIEW.md` 问题 4 中列出的改动点与优先改进项（把 `direction[] × speed` 升级为每轮系数或函数指针、给 `g_motions` 与枚举加一致性校验等）。
