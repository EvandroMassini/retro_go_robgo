IDF_TARGET = "esp32"
FW_FORMAT = "none"
DEFAULT_APPS = "launcher fmsx"
PROJECT_APPS["fmsx"][2] = 655360  # 640 KiB para incluir VGA e PS/2.
