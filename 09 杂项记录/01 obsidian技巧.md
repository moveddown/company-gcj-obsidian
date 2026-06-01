
# 代码块折叠与引用

## 一、代码块折叠

Obsidian 原生最稳的做法，不是单独折叠代码块，而是把代码块放在标题下面，然后折叠标题。

开启方式：

1. 打开 `设置`
2. 进入 `编辑器`
3. 开启 `Fold heading` / `折叠标题`
4. 鼠标移动到标题左侧，点击小箭头折叠

也可以用命令面板：

1. 按 `Ctrl + P`
2. 搜索 `Fold all headings and lists`
3. 执行后折叠所有标题和列表

推荐写法：

````md
### SHT30_Read_Humiture

```c
uint8_t SHT30_Read_Humiture(float *temp, float *rh)
{
    // 这里放核心代码
}
```
````

这样 `SHT30_Read_Humiture` 这个标题下面的代码就可以跟着标题一起折叠。

## 二、引用标题下的代码

如果代码块在某个标题下面，其他文件可以直接引用这个标题。

源文件写法：

````md
### SHT30_Read_Humiture

```c
uint8_t SHT30_Read_Humiture(float *temp, float *rh)
{
    // 这里放核心代码
}
```
````

其他文件嵌入显示：

```md
![[04 SHT30-驱动代码#SHT30_Read_Humiture]]
```

只跳转，不嵌入：

```md
[[04 SHT30-驱动代码#SHT30_Read_Humiture]]
```

## 三、引用具体代码块

如果不想单独加标题，也可以给某个代码块加 block id。

源文件写法：

````md
```c
#define SHT30_READ_HUMITURE 0x2c06
```
^sht30-cmd-read-humiture
````

其他文件嵌入显示：

```md
![[04 SHT30-驱动代码#^sht30-cmd-read-humiture]]
```

只跳转，不嵌入：

```md
[[04 SHT30-驱动代码#^sht30-cmd-read-humiture]]
```

## 四、使用建议

- 大段代码：用标题包起来，然后引用标题。
- 小片段代码：用 block id。
- 常用函数：标题名直接用函数名，例如 `### SHT30_Read_Humiture`。
- block id 建议用英文短名，例如 `^sht30-read`、`^qma6100-init`。
- 代码块要写语言名，例如 `c`，这样语法高亮更清楚。

# 好用的语法记录

## 一、Callout 标注块

Callout 是 Obsidian 里的提示块、标注块，适合记录注意事项、技巧、问题、结论和代码示例。

基本写法：

```md
> [!note]
> 这里写内容
```

常用类型：

- `[!note]`：普通笔记
- `[!tip]`：技巧、建议
- `[!warning]`：注意事项
- `[!danger]`：危险、严重问题
- `[!info]`：信息说明
- `[!todo]`：待办事项
- `[!success]`：成功、结论
- `[!question]`：问题、待确认
- `[!bug]`：踩坑、异常问题
- `[!example]`：示例
- `[!summary]`：总结

自定义标题：

```md
> [!tip] 我的建议
> 这里写建议内容
```

默认展开：

```md
> [!note]+ 可展开说明
> 这里是内容。
```

默认折叠：

```md
> [!note]- 默认折叠
> 点标题可以展开。
```

适合驱动笔记的写法：

````md
> [!summary] 初始化流程
> 1. GPIO 初始化
> 2. I2C 初始化
> 3. 读取芯片 ID
> 4. 配置量程和 ODR

> [!warning] 注意事项
> 进入低功耗前，如果传感器断电，SCL/SDA 上拉电源要确认不会倒灌。

> [!example] 核心代码
> ```c
> QMA6100_Init();
> QMA6100_ReadXYZ(&x, &y, &z);
> ```
````

使用建议：

- `[!tip]` 记经验技巧。
- `[!warning]` 记硬件、低功耗、上电时序等注意事项。
- `[!bug]` 记踩坑和异常现象。
- `[!question]` 记待确认问题。
- `[!example]` 记代码示例。
- `[!summary]` 记章节结论。





