import os
import cv2
import numpy as np

# 현재 작업 디렉토리
CurrentDir = os.getcwd()
OutputDir = os.path.join(CurrentDir, "converted_textures")

# 출력 폴더가 없으면 생성
os.makedirs(OutputDir, exist_ok=True)

# 텍스처 확장자 목록
TextureExtensions = [".png", ".jpg", ".jpeg", ".tga", ".bmp", ".tif", ".tiff"]

# 모든 텍스처 파일 처리
for FileName in os.listdir(CurrentDir):
    _, Ext = os.path.splitext(FileName)
    if Ext.lower() not in TextureExtensions:
        continue

    FilePath = os.path.join(CurrentDir, FileName)
    Image = cv2.imread(FilePath, cv2.IMREAD_UNCHANGED)

    if Image is None:
        print(f"Failed to read image: {FileName}")
        continue

    # BGR 순서에서 blue 채널을 1로 설정
    if Image.shape[2] >= 3:  # 최소 BGR인 경우에만 처리
        Image = Image.astype(np.float32) / 255.0
        Image[:, :, 0] = 1.0  # blue 채널
        Image = np.clip(Image * 255.0, 0, 255).astype(np.uint8)

    OutputPath = os.path.join(OutputDir, FileName)
    cv2.imwrite(OutputPath, Image)

print("모든 텍스처 변환이 완료되었습니다.")
