install:
	sudo apt install gnome-screensaver python3 python3-pip cmake
	pip3 install opencv-python
	pip3 install face_recognition
	mkdir -p $(HOME)/.config/autostart
	mkdir -p $(HOME)/.face-authenticator/authorized
	cp authenticate.py $(HOME)/.face-authenticator/authenticate.py
	cp start.sh $(HOME)/.face-authenticator/start.sh
	chmod +x $(HOME)/.face-authenticator/start.sh
	envsubst < face-authenticator.desktop > $(HOME)/.config/autostart/face-authenticator.desktop 
	chmod +x $(HOME)/.config/autostart/face-authenticator.desktop 

authorize:
	cp $(image) $(HOME)/.face-authenticator/authorized/admin.jpg