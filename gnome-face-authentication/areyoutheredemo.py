import face_recognition
import cv2
import subprocess
import time


# Get a reference to webcam #0 (the default one)
video_capture = cv2.VideoCapture(0)

# Load the authorized picture and learn how to recognize it.
authorized_image = face_recognition.load_image_file("authorized/admin.jpg")
authorized_face_encoding = face_recognition.face_encodings(authorized_image)[0]

# Initialize some variables
process_this_frame = 0
timer = time.time()


def authorize_face_encodings(face_encodings, authorized_face_encoding):
    for face_encoding in face_encodings:
        # See if the face is a match for the known face(s)
        matches = face_recognition.compare_faces(
            [authorized_face_encoding], face_encoding)

        # If a match was found in known_face_encodings, just use the first.
        if True in matches:
            return True
    return False


def lock():
    subprocess.call(["gnome-screensaver-command", "-l"])


def unlock():
    subprocess.call(["gnome-screensaver-command", "-d"])


try:
    while True:
        # Grab a single frame of video
        ret, frame = video_capture.read()

        # Only process every other frames of video to save time
        if process_this_frame % 2 == 0:
            # Resize frame of video to 1/4 size for faster face recognition
            small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)

            # Convert the image from BGR color (which OpenCV uses) to RGB.
            rgb_small_frame = small_frame[:, :, ::-1]
            # Find all the faces and face encodings in the current frame
            face_locations = face_recognition.face_locations(rgb_small_frame)
            face_encodings = face_recognition.face_encodings(
                rgb_small_frame, face_locations)

            authorized_user_present = authorize_face_encodings(
                face_encodings, authorized_face_encoding)

            if(authorized_user_present):
                timer = time.time()
                unlock()
            elif(time.time() - timer > 5):
                lock()

            print("Authorized user present: ",
                    authorized_user_present)

        process_this_frame += 1


finally:
    # Release handle to the webcam
    video_capture.release()
    cv2.destroyAllWindows()
