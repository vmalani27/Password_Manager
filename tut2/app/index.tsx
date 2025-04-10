import { StyleSheet, View, Text } from 'react-native';

export default function HomeScreen() {
  return (
    <View style={styles.container}> 
      <Text style={styles.text}>
        Hello world
      </Text>
      <Text style={styles.text}>
        I am god
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center", // Centers vertically
    alignItems: "center", // Centers horizontally
  },
  text: {
    textAlign: 'center', // Centers text within the Text component
  },
});