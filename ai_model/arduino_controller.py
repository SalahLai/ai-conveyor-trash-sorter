import cv2
import numpy as np
from tensorflow.keras.models import load_model

import serial
import time

# # Initialize serial connection
# arduino = None
# try:
#     ports = serial.tools.list_ports.comports()
#     for port in ports:
#         try:
#             arduino = serial.Serial(port.device, 115200, timeout=.1)  # Set timeout to 0 for non-blocking
#             print(f"Connected to {port.device}")
#             break
#         except:
#             continue
# except:
#     print("Serial initialization failed - Arduino Not Found")
#     arduino = None

arduino = serial.Serial(port='COM8',   baudrate=115200, timeout=.1)

# Load your pre-trained model

model=load_model('checkpoint.model.keras')  # Replace with your actual modcleael path
#model=load_model('weights.keras')  # Replace with your actual model path

# Define class labels
CLASS_LABELS = ['metal', 'paper', 'plastic']

# Initialize webcam
cap = cv2.VideoCapture(0)  # 0 for default camera

# Check if camera opened successfully
if not cap.isOpened():
    print("Error: Could not open webcam")
    exit()

# Create window with adjustable size
cv2.namedWindow("Webcam Classification", cv2.WINDOW_NORMAL)

while True:
    # Capture frame-by-frame
    ret, frame = cap.read()
    
    if not ret:
        print("Error: Failed to capture frame")
    resized = cv2.resize(frame, (224, 224))
    resized = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)  # Convert BGR to RGB
    normalized = resized #/ 255.0  # Normalize pixel values
    input_tensor = np.expand_dims(normalized, axis=0)  # Add batch dimension

    # Make prediction
    #predictions = model.predict(input_tensor)
    predictions = model.predict(input_tensor)
    print(f"Predictions: {predictions}")
    class_index = np.argmax(predictions[0])
    label = CLASS_LABELS[class_index]
    confidence = np.max(predictions[0])
    if(confidence < 0.5):
        label = 'Unknown'
        confidence = 0.0
    else:
        # Send the label to Arduino
        arduino.write(bytes(str(class_index),   'utf-8'))
        #arduino.write(label.encode())
        time.sleep(0.1)


    # Display results on frame
    text = f"{label} ({confidence:.2f})"
    
    # Text background for readability
    cv2.rectangle(frame, (10, 10), (400, 40), (0, 0, 0), -1)
    
    # Actual text
    cv2.putText(frame, text, (20, 30), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

    # Add FPS counter
    fps = cap.get(cv2.CAP_PROP_FPS)
    cv2.putText(frame, f"FPS: {fps:.1f}", (20, 60),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 1)

    # Display the resulting frame
    cv2.imshow('Webcam Classification', frame)

    # Break loop on 'q' key press
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Release resources
cap.release()
cv2.destroyAllWindows()
