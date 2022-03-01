public class TestArrayType {

	
	public static void main (String[] args) {
		String[] x = new String[10];

		for (int i = 0; i < x.length; i++) {
			x[i] = "a" + i;
			i++;
		}

	
		for (String a : x) {
			System.out.println(a);
		}
			
		
	}
}
