import nltk

def download_nltk_data():
    """Download required NLTK data"""
    required_packages = [
        'punkt',
        'words',
        'wordnet',
        'punkt_tab',
        'averaged_perceptron_tagger'
    ]
    
    for package in required_packages:
        try:
            if package == 'punkt_tab':
                nltk.data.find('tokenizers/punkt_tab/english/')
            else:
                nltk.data.find(f'tokenizers/{package}' if package == 'punkt' else f'corpora/{package}')
            print(f"Package '{package}' is already downloaded")
        except LookupError:
            print(f"Downloading package '{package}'...")
            nltk.download(package)
            print(f"Package '{package}' downloaded successfully")

if __name__ == '__main__':
    print("Initializing NLTK data...")
    download_nltk_data()
    print("NLTK initialization complete!") 