const firebaseConfig = {
  apiKey: "AIzaSyBv1JoIg_OF4lb5QOFOBp2yJVyYtNfW6Os",
  authDomain: "rfid-key.firebaseapp.com",
  databaseURL:
    "https://rfid-key-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "rfid-key",
  storageBucket: "rfid-key.appspot.com",
  messagingSenderId: "934005034569",
  appId: "1:934005034569:web:f2cd01a47192059a79b998",
  measurementId: "G-5ML2K5G51P",
};
firebase.initializeApp(firebaseConfig);
const database = firebase.database();
