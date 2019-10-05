import face_recognition
import cv2
import time
import subprocess
import sys

from os.path import expanduser
HOME = expanduser("~")


def authorize_face_encodings(face_encodings, authorized_face_encoding):
    for face_encoding in face_encodings:
        # See if the face is a match for the known face(s)
        matches = face_recognition.compare_faces(
            [authorized_face_encoding], face_encoding)

        # If a match was found in known_face_encodings, just use the first.
        if True in matches:
            return True
    return False


def unlock():
    subprocess.call((["gnome-screensaver-command", "-d"]))

def is_locked():
    result = str(subprocess.check_output(['gnome-screensaver-command', '-q']))
    if "is active" in result:
        return True
    return False

def is_open():
    result = str(subprocess.check_output(['cat', '/proc/acpi/button/lid/LID0/state']))
    if "open" in result:
        return True
    return False

def authenticate(video_capture):
    while is_open():
        # Grab a single frame of video
        _, frame = video_capture.read()

        # Only process every other frames of video to save time

        # Resize frame of video to 1/4 size for faster face recognition
        small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)

        # Convert the image from BGR color (which OpenCV uses) to RGB.
        rgb_small_frame = small_frame[:, :, ::-1]
        # Find all the faces and face encodings in the current frame
        face_locations = face_recognition.face_locations(rgb_small_frame)
        face_encodings = face_recognition.face_encodings(
            rgb_small_frame, face_locations)

        authorized_user_present = authorize_face_encodings(
            face_encodings,
            authorized_face_encoding)

        if(authorized_user_present):
            unlock()
            break

while True:
    if(is_locked() and is_open()):
        # Get a reference to webcam #0 (the default one)
        video_capture = cv2.VideoCapture(0)
        # Load the authorized picture and learn how to recognize it.
        authorized_image = face_recognition.load_image_file(HOME + "/.face-authenticator/authorized/admin.jpg")
        authorized_face_encoding = face_recognition.face_encodings(authorized_image)[0]
        authenticate(video_capture)
        video_capture.release()
        cv2.destroyAllWindows()
    