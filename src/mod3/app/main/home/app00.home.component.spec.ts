import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { App00HomeComponent } from './app00.home.component';

describe('App00HomeComponent', () => {
  let component: App00HomeComponent;
  let fixture: ComponentFixture<App00HomeComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ App00HomeComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(App00HomeComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
