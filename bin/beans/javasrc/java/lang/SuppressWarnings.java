package java.lang;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target({
	ElementType.TYPE,
	ElementType.FIELD,
	ElementType.METHOD,
	ElementType.PARAMETER,
	ElementType.CONSTRUCTOR,
	ElementType.LOCAL_VARIABLE
})
@Retention(RetentionPolicy.SOURCE)
public @interface SuppressWarnings
{
    String[] value();
}
