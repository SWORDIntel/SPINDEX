import sys
import spacy
import json
import os

# Example: scripts/ner_spacy.py
# Takes text as the first command-line argument.
# Prints a JSON list of extracted GPE (Geopolitical Entity) and LOC (Location)
# entities to stdout.

def main():
    if len(sys.argv) < 2:
        # print("Usage: python ner_spacy.py "<text_to_analyze>"", file=sys.stderr)
        # Output empty JSON list if no input text is provided for graceful handling by C caller
        print(json.dumps([]))
        sys.exit(0) # Not an error for the C caller, just no input.

    input_text = sys.argv[1]

    # Determine model name - could be from env var or passed differently
    # For now, hardcoding a small, efficient model.
    # Ensure this model is downloaded: python -m spacy download en_core_web_sm
    model_name = os.environ.get("SPACY_MODEL", "en_core_web_sm")

    try:
        nlp = spacy.load(model_name)
    except OSError:
        # Model not found. This is a common issue.
        # print(f"Error: spaCy model '{model_name}' not found. Please download it.", file=sys.stderr)
        # print(f"Try: python -m spacy download {model_name}", file=sys.stderr)
        # Output empty JSON list to allow C caller to proceed without locations.
        # The C side should log this as a config/setup error.
        print(json.dumps([]))
        sys.exit(1) # Indicate an error to the C caller if model isn't there

    doc = nlp(input_text)
    locations = []
    for ent in doc.ents:
        if ent.label_ in ["GPE", "LOC"]:
            locations.append(ent.text)

    # Output the list of location strings as a JSON array
    print(json.dumps(locations))

if __name__ == "__main__":
    main()
