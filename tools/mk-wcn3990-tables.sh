#!/bin/bash
# Give the kernel's build ath10k's descriptions of the WLFW messages and its WCN3990 copy-engine
# tables, for bsd/dev/hakios.wcn3990.c (the X01A's Wi-Fi start-up). They are taken out of Linux's
# drivers/net/wireless/ath/ath10k as they are (ISC) -- qmi_wlfw_v01.h with the tables of
# qmi_wlfw_v01.c, and from snoc.c the copy engines' pipes, the map of services to them and the
# shadow registers -- and written as one header into the kernel's build directory, where the
# compiler finds it and git does not. Without it hakios.wcn3990.c leaves the configuration out.
#
# usage: mk-wcn3990-tables.sh [HAKIOS_DIR]      (default ~/hakios), then build the kernel again
#   LINUX_SRC=DIR   an unpacked Linux 7.0.14 (at least drivers/net/wireless/ath); default ../ref/linux-7.0.14
set -e
HERE=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
ATH=${LINUX_SRC:-$HERE/ref/linux-7.0.14}/drivers/net/wireless/ath/ath10k
H=${1:-$HOME/hakios}
OUT=$H/xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT
[ -f "$ATH/qmi_wlfw_v01.c" ] || { echo "mk-wcn3990-tables.sh: no ath10k in $ATH (unpack linux-7.0.14 and give it as LINUX_SRC)" >&2; exit 1; }
[ -d "$OUT" ] || { echo "mk-wcn3990-tables.sh: no build directory $OUT: build the kernel once first" >&2; exit 1; }
python3 -I - "$ATH" > "$OUT/hakios.wcn3990_ath10k.h.new" <<'PY'
import re
import sys

src = sys.argv[1]
read = lambda name: open(src + '/' + name).read()
h, c, snoc, ce, htc, qmi = (read(n) for n in ('qmi_wlfw_v01.h', 'qmi_wlfw_v01.c', 'snoc.c', 'ce.h', 'htc.h', 'qmi.h'))


def cut(text, start, end):
    at = text.index(start)
    return text[at:text.index(end, at) + len(end)]


print("/* ath10k's WLFW messages and WCN3990 copy-engine tables (Linux 7.0.14, ISC), put together by")
print(" * mk-wcn3990-tables.sh: not to be edited, not to be kept. */")
print('''#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t __le16;
typedef uint32_t __le32;
#define __cpu_to_le16(x) ((uint16_t)(x))
#define __cpu_to_le32(x) ((uint32_t)(x))
#ifndef BIT
#define BIT(n) (1u << (n))
#endif
''')
# the messages: the header without its includes, then every table of the .c, in its order
print(re.sub(r'^#include [<"][^>"]*[>"]\n', '', h, flags=re.M))
print('\n'.join(re.findall(r'(?:static )?const struct qmi_elem_info \w+\[\] = \{.*?\n\};\n', c, re.S)))
# what snoc.c's tables are written in
print(cut(ce, '#define CE_ATTR_DIS_INTR', '\n'))
print(cut(ce, '#define CE_ATTR_FLAGS', '\n'))
print(cut(ce, 'struct ce_pipe_config {', '};'))
print(cut(ce, '#define PIPEDIR_NONE', '#define PIPEDIR_INOUT   3'))
print(cut(ce, 'struct ce_service_to_pipe {', '};'))
print(cut(qmi, 'struct ath10k_shadow_reg_cfg {', '};'))
print(cut(htc, 'enum ath10k_htc_svc_gid {', '};'))
print(cut(htc, '#define SVC(group, idx)', '(int)(idx))'))
print(cut(htc, 'enum ath10k_htc_svc_id {', '};'))
print('#undef SVC')
print(cut(snoc, '#define WCN3990_SRC_WR_IDX_OFFSET', '\n'))
print(cut(snoc, '#define WCN3990_DST_WR_IDX_OFFSET', '\n'))
for table in ('target_shadow_reg_cfg_map', 'target_ce_config_wlan', 'target_service_to_ce_map_wlan'):
    text = re.search(r'static (?:const )?struct \w+ %s\[\] = \{.*?\n\};\n' % table, snoc, re.S).group(0)
    print(text.replace('static struct', 'static const struct', 1))
PY
mv "$OUT/hakios.wcn3990_ath10k.h.new" "$OUT/hakios.wcn3990_ath10k.h"
echo "mk-wcn3990-tables.sh: $OUT/hakios.wcn3990_ath10k.h"
