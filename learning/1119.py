import cv2
cap = cv2.VideoCapture("/dev/video0", cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))

if not cap.isOpened():
    raise RuntimeError("无法打开摄像头")
                    
while True:
    ret, frame = cap.read()
    cv2.imshow("676767", frame)
    key = cv2.waitKey(1)
    if key != -1:
        break

cap.release()

