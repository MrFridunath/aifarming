import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { App00LoginComponent } from './app00-login.component';

describe('App00LoginComponent', () => {
  let component: App00LoginComponent;
  let fixture: ComponentFixture<App00LoginComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ App00LoginComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(App00LoginComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
