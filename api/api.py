from flask import Flask, jsonify
from flask_cors import CORS # This allows Flutter app to connect
import json
import os

app = Flask(__name__)
CORS(app) # Crucial for Flutter/Mobile development
 
DATA_FILE = "/home/pifridge/PiFridge/build/fridge_data.json"

@app.route('/api/fridge', methods=['GET'])
def get_data():
    if not os.path.exists(DATA_FILE):
        return jsonify({"error": "No data from sensors yet"}), 404
    
    try:
        with open(DATA_FILE, 'r') as f:
            data = json.load(f)
        return jsonify(data)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    # Runs on all interfaces (0.0.0.0)
    app.run(host='0.0.0.0', port=5000)

