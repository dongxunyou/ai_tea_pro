// gen_font.js — MUST be UTF-8 without BOM
const { execFileSync } = require('child_process');

const SYMBOLS = "°—·、。！（）——：；？～【】「」《》中上下不与两个为主之井享仅从仓以会传位使例保信倒停允先克入全关冲准出分创初别到制前加动化匹区卡去发取台叶号名后向否启和器回围在备失如始存它安完定实将小就已帝常并度开式强归录志总悬感成所手打投拖择按接支放故效整无时是显普有未机杯板某柑查标校检模次款止正每水汤没泡注泵洱流测浮清渣温滑烧烫热生用界的皮盖目知码砝确示秒秤称移程空箱类红约线绕继绪续绿罐置者耗自至舵花范英茶菊行装要覆认许设识试误请调败超跑跳载输达过运返还这进连选通道部配重量错闭限除障零青面页顿龙也了产任作值内再写功劲升单即厂原参口只四坐块壶处复够套好容对少干应废当往态急恢慢或把报控撞操数斗料方档浓浸消淡漏点照煮特状独理直看真瞬稍稳立等翻能舱记该读走路轴退逐都释里震靠顶顺验高默，";

const FONT = "C:\\Windows\\Fonts\\Noto Sans SC (TrueType).otf";
const FA = "D:\\temp\\opencode\\fa-solid-900.woff";
const OUTDIR = "D:\\stm32n6tea_project\\stm32-n647-brew-tea\\Appli\\User\\fonts_tmp\\";
const CLI = "C:\\Users\\86147\\AppData\\Roaming\\npm\\node_modules\\lv_font_conv\\lv_font_conv.js";

const faRanges = ["0xF001","0xF008","0xF00B-0xF00D","0xF011","0xF013","0xF015","0xF019","0xF01C","0xF021","0xF026-0xF028","0xF03E","0xF043","0xF048","0xF04B-0xF04D","0xF051-0xF054","0xF067-0xF068","0xF06E","0xF070-0xF071","0xF074","0xF077-0xF079","0xF07B","0xF093","0xF095","0xF0C4-0xF0C5","0xF0C7","0xF0C9","0xF0E0","0xF0E7","0xF0EA","0xF0F3","0xF0FB","0xF11C","0xF124","0xF15B","0xF1EB","0xF240-0xF244","0xF2ED","0xF304","0xF55A","0xF7C2"];

const sizes = [
  [14, "lv_font_cn_14", false],
  [16, "lv_font_cn_16", true],
  [24, "lv_font_cn_24", false],
];

for (const [sz, name, hasExtraRanges] of sizes) {
  const args = [
    "--font", FONT,
    "--size", String(sz),
    "--bpp", "4",
    "--format", "lvgl",
    "-r", "0x20-0x7F",
    "--symbols", SYMBOLS,
    "--no-compress", "--no-prefilter",
  ];
  // For 16px, add FA font + ranges
  if (hasExtraRanges) {{
    args.push("--font", FA);
    for (const r of faRanges) {{
      args.push("-r", r);
    }}
    args.push("--lv-include", "lvgl.h");
  }}
  args.push("-o", OUTDIR + name + ".c");
  args.push("--lv-font-name", name);

  console.log("Generating " + name + "...");
  execFileSync(process.execPath, [CLI, ...args], { stdio: "inherit" });
  console.log(name + " done.");
}

console.log("All done.");
