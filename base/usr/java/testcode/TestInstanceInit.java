
public class TestInstanceInit {
	

	private static int x = 10;

	public void set_x (int c) {
		x = c;
	}

	public int get_x () {
		return x;
	}

	public static void main (String[] args) {
		TestInstanceInit t = new TestInstanceInit();
		t.set_x(12);
		System.out.println(t.get_x());
	}
}
