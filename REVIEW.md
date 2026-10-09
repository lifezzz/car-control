# REVIEW.md — 四轮小车控制模拟

> 本文结合本仓库最终提交的代码回答题目中的 4 个问题。文中出现的文件名、函数名都与实际代码一致，可直接对照阅读。

## 问题 1：程序由哪些主要部分组成？`main` 完成了什么？

### 整体分层

程序按"数据 → 解析 → 逻辑 → 界面"分成四层，每一层只依赖它下面的一层，互不反向调用：

| 层 | 文件 | 职责 |
| --- | --- | --- |
| 配置 | `include/config.h` | 把速度范围 `0~100`、单次 `step` 变化量 `20`、token 数量与长度等常量集中在一个头文件里，避免魔法数字散落各处 |
| 运动描述层 | `include/motion.h`、`src/motion.c` | 定义"运动方式"这一概念本身：`motion_id_t` 枚举、轮位枚举 `wheel_id_t`，以及核心的 `g_motions[]` 描述表 |
| 命令解析层 | `include/command.h`、`src/command.c` | 把一行文本变成结构化的 `command_t`，并把"不合法"的原因通过 `parse_result_t` 返回；**它不碰小车状态** |
| 业务逻辑层 | `include/car.h`、`src/car.c` | 小车本身：三态状态机、自动/手动要求的存储与仲裁、四轮实际速度的动力学 |
| 交互层 | `include/cli.h`、`src/cli.c` | 读行、分发、打印 `status`、把各种结果翻译成人能看懂的提示 |

### 各部分具体内容

**1) 运动描述层（`src/motion.c`）**

这是我认为整个程序里最值得一说的地方：我没有把 `forward/left/...` 写成 `if / switch` 分支，而是把每种运动的语义压缩成一张**数据表**：

```c
const motion_desc_t g_motions[MOTION_COUNT] = {
    [MOTION_NONE]     = { "NONE",     "",         {  0,  0,  0,  0 }, false },
    [MOTION_FORWARD]  = { "FORWARD",  "forward",  {  1,  1,  1,  1 }, true  },
    [MOTION_BACKWARD] = { "BACKWARD", "backward", { -1, -1, -1, -1 }, true  },
    [MOTION_LEFT]     = { "LEFT",     "left",     { -1,  1,  1, -1 }, true  },
    [MOTION_RIGHT]    = { "RIGHT",    "right",    {  1, -1, -1,  1 }, true  },
    [MOTION_STOP]     = { "STOP",     "stop",     {  0,  0,  0,  0 }, false }
};
```

每行四个数字就是左前、右前、左后、右后四个轮子的方向系数，最后一列表示该运动是否需要 `<speed>` 参数（只有 `stop` 不需要）。

围绕这张表只有三个小函数：

- `motion_parse()`：大小写不敏感地查表，把输入词映射成 `motion_id_t`；
- `motion_name()`：给 `status` 输出大写名称；
- `motion_wheel_speeds()`：把 `motion + speed` 展开成四轮目标速度（`direction[i] * speed`）。

好处是**"运动的语义"只存在于一个地方**：解析、输出、目标速度计算全都读同一张表，不存在"解析时认识 left、算速度时忘了 left"这种不一致。

**2) 命令解析层（`src/command.c`）**

- `tokenize()`：按空白切词，最多 8 个 token、每个 31 字符，超限直接返回 `-1`（对应 `PARSE_TOO_LONG`），因此不可能有缓冲区溢出；
- `parse_speed()`：用 `strtol(token, &end, 10)` 并要求 `*end == '\0'`（拒绝 `60abc`、`60.5`、空串），再检查范围是否落在 `CAR_SPEED_MIN..CAR_SPEED_MAX`；
- `parse_control_request()`：`auto` 和 `manual` 语法完全相同，所以共用一个函数，用 `is_auto` 参数区分；`manual release` 在这里被识别（它优先于"运动词"判断，因为 `release` 不是运动）；
- `command_parse()`：先识别无参数命令（`start/step/emergency/reset/status/quit/exit`），再处理 `auto/manual` 家族，其余一律 `PARSE_UNKNOWN_COMMAND`。每种命令的参数个数都做了检查（少了给 `PARSE_MISSING_ARGUMENT`，多了给 `PARSE_UNEXPECTED_ARGUMENT`）。

这一层的输出是一个 `command_t`：只有"用户想要什么"，不包含任何执行结果。

**3) 业务逻辑层（`src/car.c`）**

核心数据结构是 `car_t`：

```c
typedef struct {
    car_state_t state;        /* IDLE / RUNNING / EMERGENCY */
    requirement_t auto_req;   /* 保存的自动控制要求 */
    requirement_t manual_req; /* 保存的手动控制要求；present == 手控是否生效 */
    int wheels[WHEEL_COUNT];  /* 四个轮子的实际速度 */
} car_t;
```

一个"要求"就是 `requirement_t{present, motion, speed}`：`present` 表示"有没有这条要求"，`motion = MOTION_STOP` 表示"有要求，内容是停止"。这个三件套很好地表达了题目里 `AUTO NONE` / `AUTO STOP` / `AUTO FORWARD 60` 三种输出的区别。

对外提供两类接口：

- **操作类**：`car_start` / `car_set_auto` / `car_set_manual` / `car_release_manual` / `car_step` / `car_emergency` / `car_reset`，返回值是 `car_result_t`，让界面层知道"是成功了、还是因为状态不对被拒绝、还是因为没有手控可释放"，从而打印不同的话；
- **查询类**：`car_effective()`（谁在生效 + 生效的要求是什么）、`car_target_wheels()`（当前四轮目标速度）、`car_state_name()` / `car_control_name()`（给输出用的名字）。

**4) 交互层（`src/cli.c`）**

- `cli_run()` 是主循环：`fgets` 逐行读 → 处理"行比缓冲区还长"的情况（丢到换行符并提示，不把后半行当成新命令）→ 去掉 `\r\n` → 交给 `command_parse()` → 解析失败走 `report_parse_error()`，成功则 `execute()`；
- `execute()` 是一个 `switch (command->type)`，把每种命令翻译成对应的 `car_*` 调用 + 提示文字；
- `print_status()` 负责严格格式的 6 行输出；
- 一个容易被忽略但很重要的约定：**stdout 只写 `status` 的 6 行，所有提示都写 stderr**。题目说除 `status` 外格式自定，但又要求 `status` 行间不得夹杂提示文字；把两者分到不同流以后，`./car < test.txt > out.txt` 得到的文件正好就是若干 status 块的拼接，可以逐字比对，而交互时终端依旧能看到全部提示。

### `main` 中完成的工作

`main` 只有 4 件事（`src/main.c`）：

1. 声明一个 `car_t` 并调用 `car_init()`，让它处于题目要求的初始状态：`IDLE`、无任何要求、四轮实际速度 0；
2. 用 `cli_is_interactive()` 判断标准输入是不是终端，如果是（人在交互）就打印一次命令提示 banner；
3. 把 `stdin/stdout/stderr` 三个流交给 `cli_run()`，由它跑完整个读-解析-执行循环；
4. `return 0`。

也就是说 `main` 里**没有业务逻辑**：状态机、控制优先级、速度变化都在 `car.c`，文本处理在 `cli.c`。这样做的好处是业务逻辑不依赖输入输出方式（管道、文件、将来的网络或图形界面都能复用同一套 `car_*` 接口），也让这一层的逻辑更容易单独推理和测试。

## 问题 2：手动控制如何优先？`manual release` 后如何恢复自动要求？

### 手动优先的实现方式

关键在于**不做"抢占式覆盖"，而是每次现算**。`auto` 和 `manual` 两条命令分别只写自己那一条记录：

```c
car_result_t car_set_auto(car_t *car, motion_id_t motion, int speed)
{
    if (car->state != STATE_RUNNING) return CAR_ERR_NOT_RUNNING;
    store_requirement(&car->auto_req, motion, speed);   /* 只动 auto_req */
    return CAR_OK;
}
```

`car_set_manual()` 同理，只写 `car->manual_req`。两者互不干扰，所以"手控生效期间允许更新自动要求"这件事不需要写任何额外代码就自然成立了。

真正体现优先级的只有 `car_effective()` 这一个函数：

```c
void car_effective(const car_t *car, control_source_t *source, requirement_t *requirement)
{
    if (car->manual_req.present) {            /* 1. 有手控 -> 手控赢 */
        chosen_source = CONTROL_MANUAL;
        chosen_request = car->manual_req;
    } else if (car->auto_req.present) {       /* 2. 否则退到自动 */
        chosen_source = CONTROL_AUTO;
        chosen_request = car->auto_req;
    } else {                                  /* 3. 都没有 -> NONE */
        chosen_source = CONTROL_NONE;
        chosen_request = requirement_absent();
    }
    ...
}
```

`CONTROL`、`TARGET` 两行输出以及 `step` 的目标速度，全部只有一个来源：`car_effective()`（`car_target_wheels()` 就是先调它、再用 `motion_wheel_speeds()` 展开）。这样"手动优先"这条规则在代码里只写了一遍，不会出现"输出说 MANUAL、算速度却按 AUTO 算"的矛盾。

`manual stop` 之所以仍然让 `CONTROL` 为 `MANUAL`、`TARGET` 为 `STOP`，也是这个模型的自然结果：它执行的是 `store_requirement(&car->manual_req, MOTION_STOP, 0)`，即"手控**存在**，内容是停止"，所以 `manual_req.present == true`，手控依然在生效。

### `manual release` 如何恢复到之前的自动要求

`car_release_manual()` 只做一件事——把手控记录置为"不存在"：

```c
car_result_t car_release_manual(car_t *car)
{
    if (!car->manual_req.present) return CAR_ERR_NO_MANUAL;
    car->manual_req = requirement_absent();   /* present = false, motion = MOTION_NONE */
    return CAR_OK;
}
```

因为自动要求**从来没有被覆盖或删除过**（`auto` 命令只写 `auto_req`），释放之后 `car_effective()` 中 `if (car->manual_req.present)` 不成立，就自动落到第二个分支，取到 `car->auto_req` ——也就是手动期间更新过的那条最新自动要求。

题目里的例子我是这样验证的（见 `tests/cases/03_manual_release_restores_auto.in`）：

```text
start
auto forward 60        auto_req = FORWARD 60
manual left 30         manual_req = LEFT 30      -> CONTROL MANUAL, TARGET LEFT 30
auto backward 80       auto_req = BACKWARD 80    -> CONTROL 仍是 MANUAL
step                   实际速度：-20 20 20 -20（按 LEFT 30 的目标算）
manual release         manual_req 清空           -> CONTROL AUTO, TARGET BACKWARD 80
status
```

`status` 输出 `CONTROL AUTO` / `AUTO BACKWARD 80` / `MANUAL NONE` / `TARGET BACKWARD 80`，和"释放后切换到 backward 80"的要求一致。

换句话说，**恢复不需要"恢复"这个动作**：因为手控生效期间自动要求一直在被独立更新，释放只是一个删除动作，优先级链本身就会给出正确结果。这样写也顺带解决了一个容易出错的点——如果把 release 写成"把保存的自动要求再拷贝回当前目标"，就很容易漏掉"手控期间自动要求被更新过"的情况。

## 问题 3：普通运动命令、`step`、`emergency` 对实际速度的影响有什么区别？

| | 普通运动命令（`auto` / `manual`） | `step` | `emergency` |
| --- | --- | --- | --- |
| 影响对象 | 只改**要求**（`auto_req` / `manual_req`） | 只改**实际速度**（`wheels[]`），朝目标逼近 | 立即把**实际速度**清零 |
| 实际速度 | 完全不变 | 每个轮子最多变化 20 | 直接变成 `0 0 0 0` |
| 是否受状态限制 | 是，只有 `RUNNING` 才接受 | 是，只有 `RUNNING` 才接受 | 否，任何状态都能执行 |
| 是否改变状态 | 否 | 否 | 是，状态锁存为 `EMERGENCY` |
| 处理位置 | `car_set_auto()` / `car_set_manual()` | `car_step()`（目标来自 `car_target_wheels()`） | `car_emergency()` |

**1) 普通运动命令：只立目标，不动速度。**
`car_set_auto()` / `car_set_manual()` 里只有状态检查和两次赋值，一行都没有碰 `car->wheels`。所以下完命令立刻 `status`，能看到 `TARGET` 已经变了而 `WHEELS` 还是旧值。题目示例里 `auto forward 60` 之后 `WHEELS 0 0 0 0` 就是这一点的体现。

**2) `step`：唯一的"实际速度"入口。**

```c
car_result_t car_step(car_t *car)
{
    int target[WHEEL_COUNT];
    if (car->state != STATE_RUNNING) return CAR_ERR_NOT_RUNNING;

    car_target_wheels(car, target);                 /* 目标 = 当前生效要求 */

    for (int i = 0; i < WHEEL_COUNT; ++i) {
        int delta = target[i] - car->wheels[i];     /* 差值 */
        if (delta > CAR_STEP_DELTA) delta = CAR_STEP_DELTA;
        else if (delta < -CAR_STEP_DELTA) delta = -CAR_STEP_DELTA;
        car->wheels[i] += delta;                    /* 每次最多 20 */
    }
    return CAR_OK;
}
```

用 `delta = 目标 - 当前` 再对差值做 ±20 的**裁剪**，而不是"直接向目标跳"，就天然满足了"变化时必须正确经过 0，不得跳变"：当前 40、目标 −40 时差值是 −80，被裁成 −20，于是依次得到 20 → 0 → −20 → −40（`tests/cases/05_step_crosses_zero.out` 覆盖了这条路径）。

另外，目标速度本身是每次 `step` 时通过 `car_target_wheels()` → `car_effective()` 现算的，所以手控释放、`auto` 更新、`manual stop` 这些"只改要求"的命令，会在下一次 `step` 时立刻生效。

**3) `emergency`：唯一的"绕过 `step`"的路径。**

```c
void car_emergency(car_t *car)
{
    car->state = STATE_EMERGENCY;
    for (int i = 0; i < WHEEL_COUNT; ++i) {
        car->wheels[i] = 0;          /* 立即归零，不经过 step */
    }
    /* 已保存的 auto / manual 要求保留：清空要求是 reset 的职责 */
}
```

它做了两件事：把状态锁存为 `EMERGENCY`、把四轮实际速度直接清零。为什么不顺手把 `auto_req` / `manual_req` 也清空？因为题目把"清空自动/手动要求"明确写在了 `reset` 的语义里，而 `emergency` 只说了"四个车轮的实际速度立即变为 0"。所以我让两者分工：`emergency` 负责"停"和"锁"，`reset` 负责"清"。

急停之后轮子不会再动，靠的是状态门禁：`car_step()`、`car_set_auto()`、`car_set_manual()` 开头都要求 `state == STATE_RUNNING`，而 `car_start()` 在 `EMERGENCY` 下返回 `CAR_ERR_EMERGENCY`（要重新运行必须先 `reset`）。也就是说急停后用 `status` 看，`WHEELS` 必然是 `0 0 0 0`，且任何"想让它重新动起来"的命令都会被拒绝——即使 `auto` 要求还保存着。

`reset` 的实现最短，因为它和上电初始状态完全等价，所以直接复用初始化：

```c
void car_reset(car_t *car) { car_init(car); }
```

`car_init()` 做的是 `memset` + 状态置 `IDLE` + 两条要求置为 `absent`，正好对应题目要求的"回到待机、清空两种要求、四轮实际速度为 0"。把"复位"和"初始化"统一成一个函数，可以避免两处逻辑日后不同步（例如以后加了新字段只改一处）。

## 问题 4：以后增加一种新的运动方式，需要改哪些地方？

### 比较方便的地方

**核心逻辑基本上"零改动"**。因为在 `motion.c` 里运动是数据而不是分支，新增一种运动只需要：

1. 在 `include/motion.h` 的 `motion_id_t` 中加一个枚举值（比如 `MOTION_TURN_AROUND`，插在 `MOTION_STOP` 之前、保持 `MOTION_NONE = 0` 与 `MOTION_COUNT` 结尾的约定）；
2. 在 `src/motion.c` 的 `g_motions[]` 中加一行：名称 `"TURN_AROUND"`、输入词 `"turn"`、四个方向系数、是否带速度。

加完之后，**解析（`motion_parse`）、`status` 的大写输出（`motion_name`）、四轮目标速度（`motion_wheel_speeds`）都会自动生效**，`car.c` 和 `cli.c` 一行都不用改。这也正是我选"描述表 + 方向系数"这个设计的原因：题目明确提醒"后续可能增加新的运动方式或控制方式"，我不希望每加一种运动都要在四五个 switch 里搜一圈。

### 修改起来比较麻烦的地方

1. **枚举和表可能不同步（我认为这是当前代码最大的隐患）。** `g_motions` 是 `extern` 数组，如果只在 `motion_id_t` 里加了枚举值却忘了在表里加行，C 语言**不会报错**：数组长度是 `MOTION_COUNT`，漏掉的位置会被零初始化，于是 `motion_name()` 返回 `NULL`、`status` 打印 `%s` 时行为未定义（glibc 下打印 `(null)`，其他实现下可能直接崩溃）。这是"用数据表换掉 switch"付出的代价——switch 漏了分支还能靠 `-Wswitch` 提醒，数组漏了元素编译器不管。
2. **方向系数模型表达力有限。** 现在一种运动只能是"四轮共用同一个速度、各自乘 ±1"。如果要加的运动出现"四个轮子速度大小不同"（比如原地旋转、阿克曼转向"内侧轮慢外侧轮快"、或者沿弧线行驶），`direction[i] * speed` 就不够了，`motion_desc_t` 需要升级。
3. **带额外参数的运动语法要动解析层。** 现在 `auto/manual` 的参数形状被写死成"一个运动词 + 可选一个 speed"。如果新运动需要两个参数（例如 `auto curve 60 30`），`parse_control_request()` 里那一串参数个数判断就要重写，`command_t` 也要加字段。
4. **`cli.c` 的提示文字依赖枚举名。** 提示里的 `AUTO STOP`、`MANUAL RIGHT 100` 是通过 `motion_name()` 拿的，这部分没问题；但 `car_state_name()` / `car_control_name()` 这类 switch 是"穷举枚举"的写法，将来给状态机加状态/控制来源时容易漏（目前有 `default: return "UNKNOWN"` 兜底，不会崩，但会静默输出 `UNKNOWN`）。
5. **测试要手工补。** 新增运动的用例需要自己写进 `tests/cases/`，没有自动生成。

### 如果继续完善，我会优先改进什么

1. **消除"枚举与表不同步"的隐患（最优先）。** 两个做法：一是把运动表用 X-Macro 定义，让枚举和表由同一份列表生成，从结构上不可能不一致；二是退一步，加一个编译期/启动期的断言，例如 `_Static_assert(sizeof(g_motions)/sizeof(g_motions[0]) == MOTION_COUNT, "...")` 再加上"每行的 `name` / `token` 非空"的运行时自检，一旦漏写就在启动时报错而不是等到 `status` 崩掉。
2. **把四轮速度模型抽象成"每轮系数"或函数指针。** 例如把 `int direction[WHEEL_COUNT]` 换成 `void (*wheel_speeds)(int speed, int out[WHEEL_COUNT])`，或者在描述里允许 `int millis[WHEEL_COUNT]`（千分比系数）；这样原地旋转、差速转向都能直接表达，也为"每个轮子独立的闭环控制"留出接口。
3. **统一的参数校验框架。** 把"命令 → 参数模板"也做成数据（每个命令声明自己接受几个位置参数、参数类型是什么），让 `command_parse()` 从一串手写的 `count != 1` 判断变成查表校验，那么以后加带多参数的命令就只需改模板。
4. **错误码与提示再细化。** 现在 `car_result_t` 已经能区分四种失败原因，可以继续扩展（例如 `speed_out_of_range` 与 `not_an_integer` 分开），让提示更精确，也便于自动测试断言具体错误类型。
5. **补测试与静态检查。** 在现有 13 组回归用例的基础上，补充"随机命令序列 + 不变量检查"的压力测试（例如：`|wheels[i]| ≤ 100`、每步变化量不超过 20、`EMERGENCY` 下 `WHEELS` 必须为 0），并在 `make test` 里加上 `-fsanitize=address,undefined` 的一轮编译运行，把"用数据表和指针"带来的风险降到最低。
