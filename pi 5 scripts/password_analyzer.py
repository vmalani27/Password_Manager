import numpy as np
# import tflite_runtime.interpreter as tflite

class PasswordStrengthAnalyzer:
    def __init__(self):
        # Initialize with some basic password features
        self.features = {
            'length': 0,
            'has_uppercase': 0,
            'has_lowercase': 0,
            'has_numbers': 0,
            'has_special': 0,
            'entropy': 0
        }
        
    def calculate_entropy(self, password):
        """Calculate password entropy"""
        char_set_size = 0
        if any(c.isupper() for c in password):
            char_set_size += 26
        if any(c.islower() for c in password):
            char_set_size += 26
        if any(c.isdigit() for c in password):
            char_set_size += 10
        if any(not c.isalnum() for c in password):
            char_set_size += 32
            
        if char_set_size == 0:
            return 0
        return len(password) * np.log2(char_set_size)
    
    def extract_features(self, password):
        """Extract features from password"""
        features = np.zeros(6, dtype=np.float32)
        features[0] = len(password) / 20.0  # Normalize length
        features[1] = 1.0 if any(c.isupper() for c in password) else 0.0
        features[2] = 1.0 if any(c.islower() for c in password) else 0.0
        features[3] = 1.0 if any(c.isdigit() for c in password) else 0.0
        features[4] = 1.0 if any(not c.isalnum() for c in password) else 0.0
        features[5] = self.calculate_entropy(password) / 100.0  # Normalize entropy
        return features
    
    def analyze_strength(self, password):
        """Analyze password strength and return score (0-1) and feedback"""
        features = self.extract_features(password)
        score = float(np.mean(features))  # Convert to float for JSON serialization
        
        feedback = []
        if features[0] < 0.5:
            feedback.append("Password is too short")
        if features[1] == 0:
            feedback.append("Add uppercase letters")
        if features[2] == 0:
            feedback.append("Add lowercase letters")
        if features[3] == 0:
            feedback.append("Add numbers")
        if features[4] == 0:
            feedback.append("Add special characters")
        if features[5] < 0.5:
            feedback.append("Password is not complex enough")
            
        return {
            'score': score,
            'feedback': feedback,
            'is_strong': bool(score >= 0.7)  # Convert to bool for JSON serialization
        } 