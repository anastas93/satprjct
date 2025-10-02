from pathlib import Path
import re

Import('env')

framework_dir = Path(env.PioPlatform().get_package_dir('framework-arduinoespressif32'))
if not framework_dir:
    raise SystemExit('framework-arduinoespressif32 package not found')

sdk_root = framework_dir / 'tools' / 'sdk' / 'esp32'

SDKCONFIG_REPLACEMENTS = {
    'CONFIG_ESP32_SPIRAM_SUPPORT=y': '# CONFIG_ESP32_SPIRAM_SUPPORT is not set',
    'CONFIG_SPIRAM_SUPPORT=y': '# CONFIG_SPIRAM_SUPPORT is not set',
    'CONFIG_SPIRAM=y': '# CONFIG_SPIRAM is not set',
    'CONFIG_SPIRAM_USE_MALLOC=y': '# CONFIG_SPIRAM_USE_MALLOC is not set',
    'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096': 'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0',
    'CONFIG_SPIRAM_CACHE_WORKAROUND=y': '# CONFIG_SPIRAM_CACHE_WORKAROUND is not set',
    'CONFIG_SPIRAM_CACHE_WORKAROUND_STRATEGY_MEMW=y': '# CONFIG_SPIRAM_CACHE_WORKAROUND_STRATEGY_MEMW is not set',
    'CONFIG_SPIRAM_BANKSWITCH_ENABLE=y': '# CONFIG_SPIRAM_BANKSWITCH_ENABLE is not set',
    'CONFIG_SPIRAM_BANKSWITCH_RESERVE=8': 'CONFIG_SPIRAM_BANKSWITCH_RESERVE=0',
    'CONFIG_SPIRAM_OCCUPY_HSPI_HOST=y': '# CONFIG_SPIRAM_OCCUPY_HSPI_HOST is not set',
    'CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y': '# CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH is not set',
    '# CONFIG_ESP_COREDUMP_ENABLE_TO_NONE is not set': 'CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y',
    'CONFIG_ESP_COREDUMP_ENABLE=y': '# CONFIG_ESP_COREDUMP_ENABLE is not set',
    'CONFIG_ESP_COREDUMP_LOGS=y': '# CONFIG_ESP_COREDUMP_LOGS is not set',
    'CONFIG_ESP_COREDUMP_CHECK_BOOT=y': '# CONFIG_ESP_COREDUMP_CHECK_BOOT is not set',
    'CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y': '# CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF is not set',
    'CONFIG_ESP_COREDUMP_CHECKSUM_CRC32=y': '# CONFIG_ESP_COREDUMP_CHECKSUM_CRC32 is not set',
    'CONFIG_ESP_COREDUMP_MAX_TASKS_NUM=64': 'CONFIG_ESP_COREDUMP_MAX_TASKS_NUM=0',
    'CONFIG_ESP_COREDUMP_STACK_SIZE=1024': 'CONFIG_ESP_COREDUMP_STACK_SIZE=0',
}

SDKCONFIG_H_DISABLE = [
    'CONFIG_ESP32_SPIRAM_SUPPORT',
    'CONFIG_SPIRAM_SUPPORT',
    'CONFIG_SPIRAM',
    'CONFIG_SPIRAM_USE_MALLOC',
    'CONFIG_SPIRAM_CACHE_WORKAROUND',
    'CONFIG_SPIRAM_CACHE_WORKAROUND_STRATEGY_MEMW',
    'CONFIG_SPIRAM_BANKSWITCH_ENABLE',
    'CONFIG_SPIRAM_BANKSWITCH_RESERVE',
    'CONFIG_SPIRAM_OCCUPY_HSPI_HOST',
    'CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH',
    'CONFIG_ESP_COREDUMP_ENABLE',
    'CONFIG_ESP_COREDUMP_LOGS',
    'CONFIG_ESP_COREDUMP_CHECK_BOOT',
    'CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF',
    'CONFIG_ESP_COREDUMP_CHECKSUM_CRC32',
    'CONFIG_ESP_COREDUMP_MAX_TASKS_NUM',
    'CONFIG_ESP_COREDUMP_STACK_SIZE',
]

SDKCONFIG_H_ENABLE = {
    'CONFIG_ESP_COREDUMP_ENABLE_TO_NONE': '1',
}

def patch_plain_config(path: Path):
    text = path.read_text(encoding='utf-8')
    original = text
    for old, new in SDKCONFIG_REPLACEMENTS.items():
        text = text.replace(old, new)
    if text != original:
        path.write_text(text, encoding='utf-8')


def patch_header_config(path: Path):
    text = path.read_text(encoding='utf-8')
    original = text
    for symbol in SDKCONFIG_H_DISABLE:
        pattern = re.compile(rf'^(#define\s+{re.escape(symbol)}\b.*)$', re.MULTILINE)
        text = pattern.sub(f'/* #undef {symbol} */', text)
    for symbol, value in SDKCONFIG_H_ENABLE.items():
        pattern = re.compile(rf'^/\* #undef {re.escape(symbol)} \*/$', re.MULTILINE)
        repl = f'#define {symbol} {value}'
        if pattern.search(text):
            text = pattern.sub(repl, text)
        elif f'#define {symbol} ' not in text:
            text += f'\n{repl}\n'
    if text != original:
        path.write_text(text, encoding='utf-8')

patch_plain_config(sdk_root / 'sdkconfig')

for variant in ('qio_qspi', 'qout_qspi', 'dio_qspi', 'dout_qspi'):
    header = sdk_root / variant / 'include' / 'sdkconfig.h'
    if header.exists():
        patch_header_config(header)
