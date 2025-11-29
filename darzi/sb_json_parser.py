import json

# Define the path to the JSON file
def process_json(json_file_path):
  # Open and read the JSON file
  with open(json_file_path, 'r') as file:
    # Parse the JSON data into a Python dictionary
    data = json.load(file)

  # Print the parsed data
  print("Parsed JSON data:")
  print(json.dumps(data, indent=2))  # Print with pretty formatting
  
  return data

