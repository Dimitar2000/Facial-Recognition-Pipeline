.PHONY:	compute_embeddings \
		build	           \
		run_webcam         \
		run_mp4            \
		clean

YUNET 		 = models/face_detection_yunet_2026may.onnx
SFACE 		 = models/face_recognition_sface_2021dec.onnx
FACE_DATASET = dataset/
EMBEDDINGS 	 = dataset_embeddings/embeddings.yaml

$(EMBEDDINGS): 
	python compute_embeddings.py    \
		--dataset $(FACE_DATASET) 	\
		--encodings $(EMBEDDINGS)   \
		--yunet-model $(YUNET) 		\
		--sface-model $(SFACE)

compute_embeddings: $(EMBEDDINGS)

build:
	rm -rf build/
	cmake -S . -B build -DCMAKE_BUILD_TYPE=$(type)

run_mp4: $(EMBEDDINGS)
	make -C build
	build/facial-recog-pipeline \
		mp4 					\
		$(fps)					\
		$(EMBEDDINGS)  			\
		$(YUNET) 				\
		$(SFACE)				\
		$(video)

run_webcam: $(EMBEDDINGS)
	make -C build
	build/facial-recog-pipeline \
		webcam 					\
		$(fps)					\
		$(EMBEDDINGS)  			\
		$(YUNET) 				\
		$(SFACE)
	
clean:
	rm -rf build
	rm -f $(EMBEDDINGS)

