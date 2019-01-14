import { ValidatorFn, FormGroup, ValidationErrors } from '@angular/forms';

export const matchPasswordValidator: ValidatorFn = (control: FormGroup): ValidationErrors | null => {
  const password = control.value.password;
  const repeat_password = control.value.repeat_password;
  return password === repeat_password ? null : { 'invalidmatch': true };
};