import unittest
from nlp_analyzer import NLPPasswordAnalyzer

class TestPasswordAnalyzer(unittest.TestCase):
    def setUp(self):
        self.analyzer = NLPPasswordAnalyzer()

    def test_password_strength(self):
        # Test cases format: (password, expected_is_strong, expected_score_range, description)
        test_cases = [
            # Strong passwords
            ('P@ssw0rd123!XYZ', True, (0.7, 1.0), 'Complex password with mixed chars'),
            ('Tr0ub4dour&3', True, (0.6, 1.0), 'Classic strong password'),
            ('VanshMalani@9426067191', True, (0.6, 1.0), 'Real-world strong password'),
            ('K33p!ng_S3cur3', True, (0.6, 1.0), 'Password with special chars and numbers'),
            
            # Weak passwords
            ('password123', False, (0.3, 0.6), 'Common word with numbers'),
            ('qwerty12345', False, (0.2, 0.5), 'Keyboard pattern with numbers'),
            ('abcdef123456', False, (0.2, 0.5), 'Sequential pattern'),
            ('aaaaaaa111', False, (0.1, 0.4), 'Repeated characters'),
            
            # Borderline cases
            ('Passw0rd!', False, (0.4, 0.6), 'Almost strong but too short'),
            ('SuperSecure2023', True, (0.6, 0.8), 'Barely strong enough'),
            
            # Special cases
            ('!@#$%^&*()1234', True, (0.6, 0.8), 'Special chars and numbers only'),
            ('ThisIsAVeryLongPasswordWithoutNumbers', False, (0.4, 0.6), 'Long but missing character types'),
        ]

        for password, expected_strong, score_range, description in test_cases:
            with self.subTest(password=password, description=description):
                result = self.analyzer.analyze_password_strength_nlp(password)
                self.assertEqual(result['is_strong'], expected_strong, 
                    f"Failed strength check for {description}")
                self.assertTrue(score_range[0] <= result['score'] <= score_range[1], 
                    f"Score {result['score']} not in expected range {score_range} for {description}")

    def test_pattern_detection(self):
        test_cases = [
            # Test common words
            ('password123', ['password'], [], [], [], 'Common word detection'),
            ('mypasswordisstrong', ['password', 'strong'], [], [], [], 'Multiple common words'),
            
            # Test keyboard patterns
            ('qwerty123', [], ['qwe', 'wer', 'ert', 'rty'], [], [], 'Keyboard pattern'),
            ('asdfgh', [], ['asd', 'sdf', 'dfg', 'fgh'], [], [], 'Keyboard pattern'),
            
            # Test repeated characters
            ('aaaa1234', [], [], ['a'], [], 'Repeated characters'),
            ('aabb1234', [], [], [], [], 'Not enough repetition'),
            
            # Test sequential numbers
            ('abc12345', [], [], [], ['123', '234', '345'], 'Sequential numbers'),
            ('pass123456', [], [], [], ['123', '234', '345', '456'], 'Multiple sequences')
        ]

        for password, exp_words, exp_keyboard, exp_repeat, exp_seq, description in test_cases:
            with self.subTest(password=password, description=description):
                patterns = self.analyzer.analyze_password_patterns(password)
                self.assertEqual(set(patterns['common_words']), set(exp_words), 
                    f"Common words mismatch for {description}")
                self.assertEqual(set(patterns['keyboard_patterns']), set(exp_keyboard), 
                    f"Keyboard patterns mismatch for {description}")
                self.assertEqual(set(patterns['repeated_chars']), set(exp_repeat), 
                    f"Repeated chars mismatch for {description}")
                self.assertEqual(set(patterns['sequential_numbers']), set(exp_seq), 
                    f"Sequential numbers mismatch for {description}")

    def test_service_name_suggestion(self):
        test_cases = [
            # Test known services
            ('gmail.com', 'Google Gmail', 'Known service with domain'),
            ('facebook', 'Facebook', 'Known service exact match'),
            ('github.com', 'GitHub', 'Known service with domain'),
            ('youtube.com', 'YouTube', 'Known service with domain'),
            ('microsoft.com', 'Microsoft', 'Known service with domain'),
            
            # Test URL cleaning
            ('https://www.example.com', 'Example', 'URL cleaning'),
            ('http://test.service.com/path', 'Test Service', 'Complex URL'),
            ('https://my.custom-site.org/login', 'My Custom Site', 'Complex URL with path'),
            ('subdomain.example.co.uk', 'Subdomain Example', 'Multiple domain parts'),
            
            # Test capitalization and separators
            ('my_service', 'My Service', 'Underscore separation'),
            ('myService', 'My Service', 'Camel case'),
            ('my-custom-service', 'My Custom Service', 'Hyphen separation'),
            ('MY_UPPERCASE_SERVICE', 'My Uppercase Service', 'Uppercase input'),
            ('my.service.name', 'My Service Name', 'Dot separation'),
            
            # Test edge cases
            ('a-b-c', 'A B C', 'Single letter components'),
            ('my--service', 'My Service', 'Multiple separators'),
            ('   my   service   ', 'My Service', 'Extra spaces'),
            ('my.service.com/path?param=value', 'My Service', 'URL with query parameters')
        ]

        for input_name, expected, description in test_cases:
            with self.subTest(input_name=input_name, description=description):
                result = self.analyzer.suggest_service_name(input_name)
                self.assertEqual(result, expected, 
                    f"Service name suggestion failed for {description}")

    def test_password_generation(self):
        test_cases = [
            ('gmail', 'user@example.com'),
            ('facebook', 'testuser'),
            ('banking', 'secure_account'),
            ('custom_service', None)
        ]

        for service, username in test_cases:
            with self.subTest(service=service, username=username):
                # Test single password generation
                result = self.analyzer.generate_smart_password(service, username)
                self.assertIsInstance(result, dict, "Should return a dictionary")
                self.assertIn('password', result, "Should contain password")
                self.assertIn('strength', result, "Should contain strength analysis")
                
                # Verify password strength
                self.assertTrue(result['strength']['is_strong'], 
                    f"Generated password for {service} should be strong")
                self.assertGreaterEqual(len(result['password']), 12, 
                    "Password should meet minimum length")

                # Test multiple suggestions
                suggestions = self.analyzer.generate_password_suggestions(service, username)
                self.assertEqual(len(suggestions), 3, "Should generate 3 suggestions")
                self.assertTrue(all(s['strength']['is_strong'] for s in suggestions), 
                    "All suggestions should be strong")
                # Verify suggestions are unique
                passwords = [s['password'] for s in suggestions]
                self.assertEqual(len(passwords), len(set(passwords)), 
                    "All suggestions should be unique")

if __name__ == '__main__':
    unittest.main() 