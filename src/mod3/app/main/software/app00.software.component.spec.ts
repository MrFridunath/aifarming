import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { App00SoftwareComponent } from './app00.software.component';

describe('App00SoftwareComponent', () => {
  let component: App00SoftwareComponent;
  let fixture: ComponentFixture<App00SoftwareComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ App00SoftwareComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(App00SoftwareComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
