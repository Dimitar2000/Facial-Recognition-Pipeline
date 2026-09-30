import argparse
import cv2
import numpy as np
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("-i", "--dataset", required=True,
					help="path to input directory of person folders and images")
parser.add_argument("-e", "--encodings", required=True,
					help="path to output OpenCV YAML file")
parser.add_argument("--yunet-model", required=True,
					help="path to the YuNet face detection ONNX model")
parser.add_argument("--sface-model", required=True,
					help="path to the SFace face recognition ONNX model")
args = parser.parse_args()

image_extensions = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
image_paths = sorted(
	path for path in Path(args.dataset).rglob("*")
	if path.is_file() and path.suffix.lower() in image_extensions
)

print("[INFO] quantifying faces...")
detector = cv2.FaceDetectorYN_create(
	args.yunet_model, "", (320, 320), 0.85, 0.3, 5000
)
recognizer = cv2.FaceRecognizerSF_create(args.sface_model, "")
out_encodings = {}

for index, image_path in enumerate(image_paths, start=1):
	print("[INFO] processing image {}/{}: {}".format(
		index, len(image_paths), image_path
	))
	name = image_path.parent.name
	image = cv2.imread(str(image_path))
	if image is None:
		raise ValueError("Could not read image: {}".format(image_path))

	detector.setInputSize((image.shape[1], image.shape[0]))
	_, faces = detector.detect(image)

	if name not in out_encodings:
		out_encodings[name] = []
	if faces is None:
		print("[WARN] no face detected in: {}".format(image_path))
		continue

	for face in faces:
		aligned_face = recognizer.alignCrop(image, face)
		embedding = recognizer.feature(aligned_face)
		out_encodings[name].append(
			np.asarray(embedding, dtype=np.float32).reshape(1, -1)
		)

print("[INFO] serializing encodings...")
storage = cv2.FileStorage(args.encodings, cv2.FILE_STORAGE_WRITE)
if not storage.isOpened():
	raise OSError("Could not open output file: {}".format(args.encodings))

storage.startWriteStruct("faces", cv2.FileNode_SEQ)
for name, embeddings in out_encodings.items():
	storage.startWriteStruct("", cv2.FileNode_MAP)
	storage.write("name", name)
	storage.startWriteStruct("embeddings", cv2.FileNode_SEQ)
	for embedding in embeddings:
		storage.write("", embedding)
	storage.endWriteStruct()
	storage.endWriteStruct()
storage.endWriteStruct()
storage.release()