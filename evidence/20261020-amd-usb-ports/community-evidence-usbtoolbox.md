# USBToolBox 一手证据

两个仓库分开看：

- kext：`USBToolBox/kext@b9887b8dd86f3fdbb2300b2938a902ecbc436267`（2025-06-13）
- tool：`USBToolBox/tool@0c6823a2c643a4488888938451194723dc9ae29a`（2023-06-10，master HEAD）

## 1. README 明确写“不 patch 端口上限”

`kext/README.md:19`

```
This does **not** patch the port limit.
```

README 列出的能力（`kext/README.md:8-17`）只有：忽略 ACPI 端口定义、覆盖 Apple
内置 map、按 PCI 设备或控制器匹配、兼容已有 map、不硬编码 port map。没有一项是
内核阈值/掩码补丁。

## 2. `mergeProperties` 只是属性搬运

`kext/USBToolBox/USBToolBox.cpp:215-247`

```cpp
void USBToolBox::mergeProperties(IORegistryEntry* instance) {
    ...
    if (!(checkKernelArgument(bootargAppleOff) || checkProperty(propertyAppleOff))) {
        deleteProperty(this->controllerInstance, "ports");
        deleteProperty(this->controllerInstance, "port-count");
    }
    ...
        if (properties) {
            if (OSCollectionIterator* propertyIterator = OSCollectionIterator::withCollection(properties)) {
                while (OSSymbol* key = OSDynamicCast(OSSymbol, propertyIterator->getNextObject())) {
                    this->controllerInstance->setProperty(key, properties->getObject(key));
                }
            }
        }
    ...
}
```

判定：

- 先删掉控制器上的 `ports` / `port-count`；
- 再把 `IOProviderMergeProperties` 字典里的键值原样 `setProperty` 到控制器；
- 没有任何字节/内核函数 patch，也没有改动 locationID。

## 3. map 里“键名”和“硬件端口号”是两件事

`tool/Scripts/base.py:604-616`

```python
                port_index = port_name_index.setdefault(prefix, 1)
                port_name = prefix + str(port_index).zfill(4 - len(prefix))
                port_name_index[prefix] += 1

                personality["IOProviderMergeProperties"]["ports"][port_name] = {
                    "port": binascii.a2b_hex(hexswap(hex(port["index"])[2:].zfill(8))),
                    "UsbConnector": port["type"] or port["guessed"],
                }
                ...
            personality["IOProviderMergeProperties"]["port-count"] = binascii.a2b_hex(hexswap(hex(highest_index)[2:].zfill(8)))
```

判定：

- 字典键名是工具自己生成的 `HS0001` / `SS0001` / `PRT0001`（同前缀计数器），纯标签；
- 真正的端口身份是每条记录里的 `"port"`，来自硬件枚举的 `port["index"]`；
- `port-count` 取“已选中的最大硬件号”，不是记录条数。

硬件号的来源（macOS 侧）：

`tool/macOS.py:104-112`

```python
                        "name": iokit.io_name_t_to_str(iokit.IORegistryEntryGetName(port, None)[1]),
                        "comment": None,
                        "index": int(read_property(port_properties["port"], 6), 16),
                        "class": macOSUSBMap.port_class_to_type(iokit.get_class_inheritance(port)),
                        "type": None,
                        ...
                        "location_id": port_properties["locationID"],
```

`index` 读的是端口对象的 `port` 属性，`location_id` 单独读；工具从不把键名当端口号。

## 4. 稀疏 map 的边界

USBToolBox 生成 map 时只把“被选中的端口”写进 `ports`，`port-count` 写最大硬件号。
这意味着：

- 想保留硬件 16/17，必须把 `"port"` 写成 16/17，并把 `port-count` 写成 ≥17；
- 改字典键名（HS04→HS01 之类）不会改变 `"port"`，也不会改变任何逻辑编号；
- 稀疏 map 能“少建对象”，但不能让同一个控制器“多建对象”；是否超过 15 个对象仍由
  Apple 内核的计数检查决定，而 USBToolBox 明确不 patch 这个检查。

## 5. 上游文档的一句旧话

`tool/README.md:29`

```
... However, there's also port limits to deal with, and in macOS 11.3, `XhciPortLimit` is broken,
resulting in a lot more hoops to go through. If you are forced to use macOS, you should probably
use [USBMap](https://github.com/CorpNewt/USBMap) instead, as it has code to handle the port limit.
```

这句写于 2023-06 之前；OpenCore 在 2023-06-12 合入 PR #471（merge commit
`d52fc46ba650ce1afe00c354331a0657a533ef18`）补上了 `IOUSBHostFamily` part 2。
所以“11.3 起彻底坏了”对当前 OpenCore 已过时，但“USBToolBox 自己不 patch”仍然成立。
