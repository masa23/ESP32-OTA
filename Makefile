all: build install

build:
	#@echo "Building PlatformIO project..."
	platformio run
	@echo "Building frontend..."
	cd frontend && npm run build

install: install-firmware install-frontend

install-firmware:
	@echo "Uploading PlatformIO project..."
	platformio run --target upload

install-frontend:
	@echo "Copying frontend build to data/..."
	rm -rf data/web
	cp -r frontend/dist data/web
	@echo "Frontend files copied to data/"
	platformio run --target uploadfs
