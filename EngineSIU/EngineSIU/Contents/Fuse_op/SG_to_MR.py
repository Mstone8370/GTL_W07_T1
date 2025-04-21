#!/usr/bin/env python3
"""
convert_all_sg_to_mr.py  ──  v1.1
∙ 파일 뒤에 붙는 후행자를 확장:
      _albedoTexture, _baseColorTexture, _diffuseTexture
      _specTexture,   _specularTexture
      _glossTexture,  _glossinessTexture
∙ 위 세 그룹을 자동 매칭해 MR 텍스처로 변환.
"""

import re, pathlib, argparse, numpy as np
from PIL import Image

# ─── 색 공간 ──────────────────────────────────────────────────────────────────
def srgb_to_linear(c):
    c = c / 255.0
    a = 0.055
    return np.where(c <= 0.04045, c / 12.92, ((c + a) / (1 + a)) ** 2.4)

def linear_to_srgb(c):
    a = 0.055
    return np.where(c <= 0.0031308, c * 12.92, (1 + a) * c ** (1/2.4) - a)

# ─── I/O ─────────────────────────────────────────────────────────────────────
def load_rgb(path):
    return srgb_to_linear(np.asarray(Image.open(path).convert("RGB"), np.float32))

def load_gray(path):
    return np.asarray(Image.open(path).convert("L"), np.float32) / 255.0

def save_rgb(arr, path):
    Image.fromarray((np.clip(linear_to_srgb(arr), 0, 1) * 255).astype(np.uint8), "RGB").save(path)

def save_gray(arr, path):
    Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8), "L").save(path)

# ─── 정확한 메탈릭 계산 ───────────────────────────────────────────────────────
def solve_metallic(D_lin, S_lin, F0=0.04, eps=1e-6):
    S_strength  = np.max(S_lin, axis=2)                               # (H,W)
    diffuseLum  = (D_lin @ [0.2126, 0.7152, 0.0722]).astype(np.float32)
    a           = F0
    b           = diffuseLum * (1 - S_strength) / (1 - F0 + eps) + S_strength - 2*F0
    c           = F0 - S_strength
    disc        = np.clip(b*b - 4*a*c, 0.0, None)
    M           = np.where(S_strength <= F0, 0.0,
                 np.clip((-b + np.sqrt(disc)) / (2*a), 0.0, 1.0))
    return M.astype(np.float32)

# ─── 머티리얼 자동 수집 ──────────────────────────────────────────────────────
POSTFIX_MAP = {                       # key → 정규화된 종류
    "albedo":"albedo", "basecolor":"albedo", "diffuse":"albedo",
    "spec":"spec", "specular":"spec",
    "gloss":"gloss", "glossiness":"gloss",
}
#    예:  (base)_(key)Texture.ext
REGEX = re.compile(r"(.+?)_(" + "|".join(POSTFIX_MAP.keys()) + r")Texture\.[^.]+$", re.I)

def gather_materials(folder: pathlib.Path):
    mats = {}
    for p in folder.iterdir():
        if not p.is_file(): continue
        m = REGEX.match(p.name)
        if not m: continue
        base, key = m.group(1), POSTFIX_MAP[m.group(2).lower()]
        mats.setdefault(base, {})[key] = p
    # albedo, spec, gloss 모두 갖춘 것만 반환
    return {b:tex for b,tex in mats.items() if {"albedo","spec","gloss"} <= tex.keys()}

# ─── 변환 ────────────────────────────────────────────────────────────────────
def convert_material(name, tex, out_dir):
    D = load_rgb(tex["albedo"])
    S = load_rgb(tex["spec"])
    G = load_gray(tex["gloss"])

    R = 1.0 - G
    M = solve_metallic(D, S)

    oneMinusS   = 1.0 - np.max(S, axis=2, keepdims=True)
    F0d         = 0.04
    B_nonMetal  = D * oneMinusS / (1.0 - F0d + 1e-6)
    B           = (1.0 - M[...,None]) * B_nonMetal + M[...,None] * S

    save_rgb (B, out_dir / f"{name}_baseColorTexture.png")
    save_gray(M, out_dir / f"{name}_metallicTexture.png")
    save_gray(R, out_dir / f"{name}_roughnessTexture.png")
    print(f"[✓] {name}")

# ─── 실행 ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default="result", help="출력 폴더")
    args = ap.parse_args()

    src = pathlib.Path(".")
    materials = gather_materials(src)
    if not materials:
        print("※ 변환할 머티리얼(알베도·스펙·글로스 세트)을 찾지 못했습니다.")
        return

    out = pathlib.Path(args.out_dir); out.mkdir(exist_ok=True)
    for name, tex in materials.items():
        convert_material(name, tex, out)

if __name__ == "__main__":
    main()
