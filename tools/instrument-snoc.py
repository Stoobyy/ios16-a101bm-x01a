from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
if 'LX10K_SNOC_CHECKPOINTS' in s:
    s = p.with_suffix('.c.before-checkpoints').read_text()
p.with_suffix('.c.before-checkpoints').write_text(s)
a = s.index('static int ath10k_snoc_hif_start(')
z = s.index('static int ath10k_snoc_init_pipes(', a)
part = s[a:z]
for statement in ['bitmap_clear(ar_snoc->pending_ce_irqs, 0, CE_COUNT_MAX);', 'netif_threaded_enable(ar->napi_dev);', 'ath10k_core_napi_enable(ar);', 'ath10k_snoc_rx_post(ar);']:
    name = statement.split('(')[0]
    part = part.replace('\t' + statement, f'\tlx_tell("snoc-check: BEFORE {name}\\n");\n\t{statement}\n\tlx_tell("snoc-check: AFTER {name}\\n");')
part = part.replace('\t\tath10k_snoc_irq_enable(ar);', '\t{\n\t\tlx_tell("snoc-check: BEFORE IRQ enable\\n");\n\t\tath10k_snoc_irq_enable(ar);\n\t\tlx_tell("snoc-check: AFTER IRQ enable\\n");\n\t}')
s = s[:a] + part + s[z:]
s = s.replace('\t\tath10k_snoc_rx_post_pipe(&ar_snoc->pipe_info[i]);', '\t{\n\t\tlx_tell("snoc-check: RX pipe %d begin\\n", i);\n\t\tath10k_snoc_rx_post_pipe(&ar_snoc->pipe_info[i]);\n\t\tlx_tell("snoc-check: RX pipe %d end\\n", i);\n\t}')
s = s.replace('\tbitmap_clear(ar_snoc->pending_ce_irqs, 0, CE_COUNT_MAX);', '\tlx10k_ce_poll_ready(0);\n\tbitmap_clear(ar_snoc->pending_ce_irqs, 0, CE_COUNT_MAX);')
s = s.replace('\tclear_bit(ATH10K_SNOC_FLAG_RECOVERY, &ar_snoc->flags);', '\tlx_tell("snoc-check: RX posting complete; polling enabled\\n");\n\tlx10k_ce_poll_ready(1);\n\tclear_bit(ATH10K_SNOC_FLAG_RECOVERY, &ar_snoc->flags);')
s = 'extern void lx10k_ce_poll_ready(int);\n' + s
p.write_text('/* LX10K_SNOC_CHECKPOINTS */\nextern void lx_tell(const char *, ...);\n' + s)
