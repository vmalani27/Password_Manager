import re
from collections import Counter
import nltk
import random
import string
from nltk.tokenize import word_tokenize
from nltk.corpus import words, wordnet
from nltk.metrics.distance import edit_distance

class NLPPasswordAnalyzer:
    def __init__(self):
        # Download required NLTK data
        try:
            nltk.data.find('tokenizers/punkt')
            nltk.data.find('corpora/words')
            nltk.data.find('corpora/wordnet')
        except LookupError:
            nltk.download('punkt')
            nltk.download('words')
            nltk.download('wordnet')
        
        self.english_words = set(words.words())
        
    def analyze_password_patterns(self, password):
        """Analyze password for common patterns and potential vulnerabilities"""
        patterns = {
            'common_words': [],
            'keyboard_patterns': [],
            'repeated_chars': [],
            'sequential_numbers': [],
            'personal_info_patterns': [],
            'risk_score': 0
        }
        
        # Check for common words
        tokens = word_tokenize(password.lower())
        for token in tokens:
            if token in self.english_words:
                patterns['common_words'].append(token)
        
        # Check for keyboard patterns
        keyboard_rows = [
            'qwertyuiop',
            'asdfghjkl',
            'zxcvbnm'
        ]
        for row in keyboard_rows:
            for i in range(len(row)-2):
                if row[i:i+3].lower() in password.lower():
                    patterns['keyboard_patterns'].append(row[i:i+3])
        
        # Check for repeated characters
        for char, count in Counter(password).items():
            if count > 2:
                patterns['repeated_chars'].append(char)
        
        # Check for sequential numbers
        numbers = re.findall(r'\d+', password)
        for num in numbers:
            if len(num) > 2:
                for i in range(len(num)-2):
                    if int(num[i+1]) == int(num[i]) + 1 and int(num[i+2]) == int(num[i]) + 2:
                        patterns['sequential_numbers'].append(num[i:i+3])
        
        # Calculate risk score
        patterns['risk_score'] = (
            len(patterns['common_words']) * 20 +
            len(patterns['keyboard_patterns']) * 15 +
            len(patterns['repeated_chars']) * 10 +
            len(patterns['sequential_numbers']) * 15
        )
        
        return patterns
    
    def suggest_service_name(self, url_or_name):
        """Suggest standardized service name from URL or user input"""
        # Remove common prefixes and suffixes
        cleaned = re.sub(r'^(https?://)?(www\.)?', '', url_or_name.lower())
        
        # Remove query parameters and trailing slashes
        cleaned = re.sub(r'\?.*$', '', cleaned)
        cleaned = re.sub(r'/+$', '', cleaned)
        
        # Remove domain extensions but keep meaningful parts
        cleaned = re.sub(r'\.([a-z]{2,})(\/.*)?$', '', cleaned)
        
        # Check against known services first
        known_services = {
            'gmail': 'Google Gmail',
            'outlook': 'Microsoft Outlook',
            'facebook': 'Facebook',
            'instagram': 'Instagram',
            'twitter': 'Twitter',
            'linkedin': 'LinkedIn',
            'amazon': 'Amazon',
            'github': 'GitHub',
            'netflix': 'Netflix',
            'spotify': 'Spotify',
            'youtube': 'YouTube',
            'google': 'Google',
            'microsoft': 'Microsoft',
            'apple': 'Apple',
            'dropbox': 'Dropbox'
        }
        
        # Try to match against known services
        base_name = cleaned.split('.')[0]  # Get first part before any dots
        for service_key, service_name in known_services.items():
            if edit_distance(base_name, service_key) <= 2:  # Allow for minor typos
                return service_name
        
        # For unknown services, split on common separators and clean
        tokens = re.split(r'[._\-\s]+', cleaned)
        tokens = [t for t in tokens if t and not t.startswith(('com', 'org', 'net'))]
        
        # Handle camelCase
        final_tokens = []
        for token in tokens:
            # Split camelCase into separate words
            camel_split = re.findall(r'[A-Z][a-z]*|[a-z]+', token)
            final_tokens.extend(camel_split)
        
        # Remove duplicates while preserving order
        seen = set()
        final_tokens = [t for t in final_tokens if not (t in seen or seen.add(t))]
        
        # Capitalize each word
        return ' '.join(word.capitalize() for word in final_tokens if word)
    
    def enhanced_search(self, query, service_names):
        """Perform enhanced search using NLP techniques"""
        results = []
        query_tokens = word_tokenize(query.lower())
        
        for service in service_names:
            service_lower = service.lower()
            service_tokens = word_tokenize(service_lower)
            
            # Calculate similarity score
            score = 0
            
            # Exact match bonus
            if query.lower() in service_lower:
                score += 100
            
            # Token matching
            for q_token in query_tokens:
                for s_token in service_tokens:
                    # Exact token match
                    if q_token == s_token:
                        score += 50
                    # Partial token match
                    elif q_token in s_token or s_token in q_token:
                        score += 30
                    # Similar token (edit distance)
                    elif edit_distance(q_token, s_token) <= 2:
                        score += 20
                    
                    # Semantic similarity using WordNet
                    try:
                        q_synsets = wordnet.synsets(q_token)
                        s_synsets = wordnet.synsets(s_token)
                        if q_synsets and s_synsets:
                            similarity = q_synsets[0].path_similarity(s_synsets[0])
                            if similarity:
                                score += similarity * 25
                    except:
                        pass
            
            if score > 0:
                results.append({
                    'service': service,
                    'score': score
                })
        
        # Sort results by score
        results.sort(key=lambda x: x['score'], reverse=True)
        return results
    
    def analyze_password_strength_nlp(self, password):
        """Analyze password strength using NLP techniques"""
        base_score = 0
        feedback = []
        
        # Length analysis (up to 40 points)
        if len(password) < 8:
            feedback.append("Password is too short (minimum 8 characters)")
        elif len(password) >= 12:
            base_score += 40
        else:
            base_score += 20  # 8-11 characters
        
        # Character variety (up to 40 points)
        if re.search(r'[A-Z]', password): base_score += 10  # Uppercase
        if re.search(r'[a-z]', password): base_score += 10  # Lowercase
        if re.search(r'\d', password): base_score += 10     # Numbers
        if re.search(r'[!@#$%^&*(),.?":{}|<>]', password): base_score += 10  # Special chars
        
        # Pattern analysis (can reduce score)
        patterns = self.analyze_password_patterns(password)
        
        # Reduce score for patterns, but don't be too strict
        pattern_penalty = 0
        
        if patterns['common_words']:
            # Only penalize if the common word is a significant part of the password
            for word in patterns['common_words']:
                if len(word) > 3 and len(word) / len(password) > 0.5:
                    feedback.append(f"Contains common word: {word}")
                    pattern_penalty += 10
        
        if patterns['keyboard_patterns']:
            feedback.append("Contains keyboard patterns")
            pattern_penalty += 5
        
        if patterns['repeated_chars']:
            # Only penalize for characters repeated more than 3 times
            if any(password.count(char) > 3 for char in patterns['repeated_chars']):
                feedback.append("Contains too many repeated characters")
                pattern_penalty += 5
        
        if patterns['sequential_numbers']:
            # Only penalize for sequences longer than 4 digits
            if any(len(seq) > 4 for seq in patterns['sequential_numbers']):
                feedback.append("Contains long number sequences")
                pattern_penalty += 5
        
        # Cap the pattern penalty at 30 points
        pattern_penalty = min(pattern_penalty, 30)
        
        # Calculate final score (0 to 1)
        final_score = max(0, (base_score - pattern_penalty)) / 100
        
        # Add positive feedback for strong elements
        if len(password) >= 12:
            feedback.append("Good length (12+ characters)")
        if re.search(r'[A-Z]', password) and re.search(r'[a-z]', password):
            feedback.append("Good mix of upper and lowercase letters")
        if re.search(r'\d', password):
            feedback.append("Includes numbers")
        if re.search(r'[!@#$%^&*(),.?":{}|<>]', password):
            feedback.append("Includes special characters")
        
        return {
            'score': final_score,
            'feedback': feedback,
            'is_strong': final_score >= 0.6,  # Lowered threshold from 0.7 to 0.6
            'patterns': patterns
        }

    def generate_smart_password(self, service_name, username=None, min_length=12):
        """Generate a strong, contextual password based on service and username"""
        # Initialize components for password
        special_chars = '!@#$%^&*()_+-=[]{}|;:,.<>?'
        numbers = string.digits
        
        # Clean and tokenize service name
        service_tokens = re.split(r'[^a-zA-Z0-9]', service_name.lower())
        service_tokens = [t for t in service_tokens if t]
        
        # Get related words using WordNet
        related_words = []
        for token in service_tokens:
            synsets = wordnet.synsets(token)
            if synsets:
                # Get lemma names from the first synset
                related_words.extend([lemma.name() for lemma in synsets[0].lemmas()])
        
        # Build password components
        components = []
        
        # Add a modified version of a related word
        if related_words:
            word = random.choice(related_words)
            # Capitalize random letters
            modified_word = ''.join(c.upper() if random.random() > 0.6 else c.lower() 
                                  for c in word)
            components.append(modified_word)
        
        # Add random numbers
        components.append(''.join(random.choices(numbers, k=random.randint(2, 4))))
        
        # Add special characters
        components.append(''.join(random.choices(special_chars, k=random.randint(2, 3))))
        
        # Add random uppercase word if needed
        if len(''.join(components)) < min_length:
            if self.english_words:
                random_word = random.choice(list(self.english_words))
                components.append(random_word.capitalize())
        
        # Shuffle components and ensure minimum length
        password = ''.join(components)
        while len(password) < min_length:
            password += random.choice(string.ascii_letters + numbers + special_chars)
        
        # Final shuffle
        password_list = list(password)
        random.shuffle(password_list)
        final_password = ''.join(password_list)
        
        # Verify password strength
        strength = self.analyze_password_strength_nlp(final_password)
        if not strength['is_strong']:
            # If not strong enough, recursively try again
            return self.generate_smart_password(service_name, username, min_length)
        
        return {
            'password': final_password,
            'strength': strength
        }

    def generate_password_suggestions(self, service_name, username, count=3):
        """Generate password suggestions based on service name and username"""
        suggestions = []
        
        # Define character sets
        lowercase = string.ascii_lowercase
        uppercase = string.ascii_uppercase
        digits = string.digits
        special_chars = "!@#$%^&*(),.?:{}|<>"
        
        # Get service-specific words
        service_words = [word.lower() for word in re.split(r'[^a-zA-Z]', service_name) if word]
        username_parts = [part.lower() for part in re.split(r'[^a-zA-Z0-9]', username) if part]
        
        for _ in range(count):
            # Start with a random word from service name or a random word
            if service_words and random.random() < 0.5:
                base = random.choice(service_words).capitalize()
            else:
                base = random.choice(list(self.english_words)).capitalize()
            
            # Add random characters
            password = base
            
            # Add numbers (2-4 digits)
            password += ''.join(random.choice(digits) for _ in range(random.randint(2, 4)))
            
            # Add special characters (1-2 chars)
            password += ''.join(random.choice(special_chars) for _ in range(random.randint(1, 2)))
            
            # Add more random characters to meet minimum length
            while len(password) < 12:
                char_set = random.choice([lowercase, uppercase, digits, special_chars])
                password += random.choice(char_set)
            
            # Shuffle the password
            password_list = list(password)
            random.shuffle(password_list)
            password = ''.join(password_list)
            
            # Verify strength
            strength = self.analyze_password_strength_nlp(password)
            if strength['is_strong']:
                suggestions.append({
                    'password': password,
                    'strength': strength['score']
                })
            else:
                # If not strong enough, try again
                count += 1
                continue
        
        return suggestions 